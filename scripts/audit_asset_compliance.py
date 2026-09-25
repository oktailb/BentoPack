#!/usr/bin/env python3
"""
BentoPack Asset Compliance & Forensic Integrity Audit Tool.

Audits PNG atlases, JSON metadata, and engine descriptors to determine license
tier, distinguish GUI vs CLI origin, verify authentic commercial signatures,
and detect unauthorized source-code modifications / circumvention (Tamper Detection).

Supports:
- PNG ancillary tEXt/iTXt chunk inspection (Generator, Tool, License)
- Steganographic pixel analysis on Alpha == 0 pixels (GUI 'SSG', CLI 'SSC', Tampered 'SST'/'SSX')
- JSON metadata inspection & HMAC-SHA256 layout verification
- Godot 4 (.tres) header comment inspection
- Distinction between GUI per-seat exports and CLI / CI/CD automated pipeline exports
- Recursive directory scanning with legal forensic report generation
"""

import sys
import os
import zlib
import struct
import json
import hmac
import hashlib
import argparse

# Forensic Steganographic Signatures in Alpha == 0 pixels:
MAGIC_COMMUNITY_GUI_RGB = (0x53, 0x53, 0x47)  # 'S', 'S', 'G' -> GUI Community Edition
MAGIC_COMMUNITY_CLI_RGB = (0x53, 0x53, 0x43)  # 'S', 'S', 'C' -> CLI Community Edition
MAGIC_TAMPERED_GUI_RGB  = (0x53, 0x53, 0x54)  # 'S', 'S', 'T' -> Tampered / Circumvented GUI
MAGIC_TAMPERED_CLI_RGB  = (0x53, 0x53, 0x58)  # 'S', 'S', 'X' -> Tampered / Circumvented CLI

class ForensicVerdict:
    COMMERCIAL_VERIFIED = "COMMERCIAL_VERIFIED"
    COMMUNITY_EXEMPTION = "COMMUNITY_EXEMPTION"
    TAMPERED_CIRCUMVENTION = "TAMPERED_CIRCUMVENTION_DETECTED"
    CLEAN_OR_UNKNOWN    = "CLEAN_OR_UNKNOWN"

def parse_png(file_path):
    """
    Parses a PNG file to extract text chunks and analyze transparent pixels.
    """
    results = {
        "text_chunks": {},
        "stego_community_gui_count": 0,
        "stego_community_cli_count": 0,
        "stego_tampered_gui_count": 0,
        "stego_tampered_cli_count": 0,
        "clean_alpha0_count": 0,
        "total_alpha0_pixels": 0,
        "is_png": False,
        "error": None
    }

    try:
        with open(file_path, "rb") as f:
            header = f.read(8)
            if header != b"\x89PNG\r\n\x1a\n":
                return results
            results["is_png"] = True

            ihdr_data = None
            idat_chunks = []

            while True:
                chunk_header = f.read(8)
                if len(chunk_header) < 8:
                    break
                length, chunk_type = struct.unpack(">I4s", chunk_header)
                data = f.read(length)
                crc = f.read(4)

                if chunk_type == b"IHDR":
                    ihdr_data = data
                elif chunk_type == b"tEXt":
                    parts = data.split(b"\x00", 1)
                    if len(parts) == 2:
                        key = parts[0].decode("latin-1", errors="replace")
                        val = parts[1].decode("latin-1", errors="replace")
                        results["text_chunks"][key] = val
                elif chunk_type == b"IDAT":
                    idat_chunks.append(data)
                elif chunk_type == b"IEND":
                    break

            if ihdr_data and idat_chunks:
                width, height, bit_depth, color_type = struct.unpack(">IIBB", ihdr_data[:10])
                # We analyze 8-bit RGBA (color_type 6, bit_depth 8)
                if color_type == 6 and bit_depth == 8:
                    raw_idat = b"".join(idat_chunks)
                    decompressed = zlib.decompress(raw_idat)
                    bpp = 4  # bytes per pixel
                    stride = width * bpp
                    pos = 0

                    prev_row = bytearray(stride)
                    for y in range(height):
                        if pos >= len(decompressed):
                            break
                        filter_type = decompressed[pos]
                        pos += 1
                        scanline = bytearray(decompressed[pos:pos + stride])
                        pos += stride

                        # Unfilter scanline
                        if filter_type == 1:  # Sub
                            for x in range(bpp, stride):
                                scanline[x] = (scanline[x] + scanline[x - bpp]) & 0xFF
                        elif filter_type == 2:  # Up
                            for x in range(stride):
                                scanline[x] = (scanline[x] + prev_row[x]) & 0xFF
                        elif filter_type == 3:  # Average
                            for x in range(stride):
                                left = scanline[x - bpp] if x >= bpp else 0
                                up = prev_row[x]
                                scanline[x] = (scanline[x] + ((left + up) >> 1)) & 0xFF
                        elif filter_type == 4:  # Paeth
                            for x in range(stride):
                                left = scanline[x - bpp] if x >= bpp else 0
                                up = prev_row[x]
                                left_up = prev_row[x - bpp] if x >= bpp else 0
                                p = left + up - left_up
                                pa = abs(p - left)
                                pb = abs(p - up)
                                pc = abs(p - left_up)
                                pr = left if (pa <= pb and pa <= pc) else (up if pb <= pc else left_up)
                                scanline[x] = (scanline[x] + pr) & 0xFF

                        prev_row = scanline

                        # Inspect pixels with Alpha == 0
                        for px in range(0, stride, bpp):
                            r = scanline[px]
                            g = scanline[px + 1]
                            b = scanline[px + 2]
                            a = scanline[px + 3]
                            if a == 0:
                                results["total_alpha0_pixels"] += 1
                                rgb = (r, g, b)
                                if rgb == MAGIC_COMMUNITY_GUI_RGB:
                                    results["stego_community_gui_count"] += 1
                                elif rgb == MAGIC_COMMUNITY_CLI_RGB:
                                    results["stego_community_cli_count"] += 1
                                elif rgb == MAGIC_TAMPERED_GUI_RGB:
                                    results["stego_tampered_gui_count"] += 1
                                elif rgb == MAGIC_TAMPERED_CLI_RGB:
                                    results["stego_tampered_cli_count"] += 1
                                elif rgb == (0, 0, 0):
                                    results["clean_alpha0_count"] += 1

    except Exception as e:
        results["error"] = str(e)

    return results

def parse_json(file_path, secret_key=None):
    """
    Parses a JSON file (TexturePacker / Aseprite / Unity / Unreal)
    """
    results = {
        "is_bentopack_json": False,
        "app": None,
        "tool": None,
        "license": None,
        "integrity": None,
        "signature": None,
        "signature_valid": None
    }
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        meta = None
        if isinstance(data, dict):
            if "meta" in data and isinstance(data["meta"], dict):
                meta = data["meta"]
            elif "app" in data:
                meta = data

        if meta:
            app_val = meta.get("app", "")
            if "BentoPack" in app_val or "BentoPack" in app_val or "Sprite Studio" in app_val:
                results["is_bentopack_json"] = True
                results["app"] = app_val
                results["tool"] = meta.get("tool")
                results["license"] = meta.get("license")
                results["integrity"] = meta.get("integrity")
                results["signature"] = meta.get("signature")

                if results["signature"] and secret_key:
                    img_name = meta.get("image", "")
                    expected_hmac = hmac.new(secret_key.encode("utf-8"), img_name.encode("utf-8"), hashlib.sha256).hexdigest()
                    expected_sig = "comm-" + expected_hmac[:24]
                    results["signature_valid"] = (results["signature"] == expected_sig)
    except Exception:
        pass

    return results

def parse_tres(file_path):
    """
    Parses a Godot 4 .tres file for comment headers.
    """
    results = {
        "is_tres": False,
        "is_bentopack": False,
        "header_text": None,
        "tool": None,
        "is_tampered": False,
        "is_community": False
    }
    try:
        with open(file_path, "r", encoding="utf-8", errors="replace") as f:
            head = f.read(1024)
            if "[gd_resource" in head or "SpriteFrames" in head:
                results["is_tres"] = True
                if "BentoPack" in head or "BentoPack" in head:
                    results["is_bentopack"] = True
                    results["header_text"] = head[:256]
                    if "CLI" in head:
                        results["tool"] = "CLI"
                    elif "GUI" in head:
                        results["tool"] = "GUI"

                    if "Tampered" in head or "Circumvention" in head:
                        results["is_tampered"] = True
                    elif "Community Edition" in head:
                        results["is_community"] = True
    except Exception:
        pass
    return results

def audit_file(file_path, secret_key=None):
    """
    Determines compliance verdict for a given file.
    """
    ext = os.path.splitext(file_path)[1].lower()
    report = {
        "file": file_path,
        "verdict": ForensicVerdict.CLEAN_OR_UNKNOWN,
        "tool": "Unknown",
        "reasons": []
    }

    if ext == ".png":
        png_info = parse_png(file_path)
        if not png_info["is_png"]:
            return report

        text = png_info["text_chunks"]
        gen = text.get("Generator", "")
        tool = text.get("X-BentoPack-Tool", text.get("X-BentoPack-Tool", ""))
        lic = text.get("X-BentoPack-License", text.get("X-BentoPack-License", ""))
        integrity = text.get("X-SS-Integrity", "")

        # Infer tool origin
        if tool in ["GUI", "CLI"]:
            report["tool"] = tool
        elif "CLI" in gen or "CLI" in lic or png_info["stego_community_cli_count"] > 0 or png_info["stego_tampered_cli_count"] > 0:
            report["tool"] = "CLI"
        elif "GUI" in gen or "GUI" in lic or png_info["stego_community_gui_count"] > 0 or png_info["stego_tampered_gui_count"] > 0:
            report["tool"] = "GUI"

        # 1. Check for blatant Tamper tags
        if "Tampered" in gen or "Tampered" in integrity or "Circumvention" in integrity:
            report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
            report["reasons"].append(f"PNG header flagged circumvention: '{integrity}' / '{gen}' (Tool: {report['tool']})")
            return report

        # 2. Check steganographic mark in Alpha=0 pixels
        if png_info["stego_tampered_gui_count"] > 0 or png_info["stego_tampered_cli_count"] > 0:
            total_tampered = png_info["stego_tampered_gui_count"] + png_info["stego_tampered_cli_count"]
            sig_name = "SSX (CLI)" if png_info["stego_tampered_cli_count"] > 0 else "SST (GUI)"
            report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
            report["reasons"].append(f"Forensic Alpha=0 pixels contain Tampered magic signature '{sig_name}' ({total_tampered} pixels)")
            return report

        stego_comm_total = png_info["stego_community_gui_count"] + png_info["stego_community_cli_count"]
        if stego_comm_total > 0:
            is_cli_stego = (png_info["stego_community_cli_count"] > png_info["stego_community_gui_count"])
            stego_sig = "SSC (CLI)" if is_cli_stego else "SSG (GUI)"

            # If user stripped metadata but stego watermark is present
            if "Community" not in gen and not lic.startswith("Community-Exemption-Under-1M"):
                report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
                report["reasons"].append(f"Steganographic '{stego_sig}' signature present ({stego_comm_total} pixels) despite stripped Community metadata (evidence of intentional tag removal)")
            else:
                report["verdict"] = ForensicVerdict.COMMUNITY_EXEMPTION
                if is_cli_stego or report["tool"] == "CLI":
                    report["reasons"].append(f"Community CLI Edition verified via standard metadata and {png_info['stego_community_cli_count']} transparent pixels")
                    report["reasons"].append("NOTE: Automated CI/CD pipeline usage requires a Commercial CLI Automation or Enterprise Studio license.")
                else:
                    report["reasons"].append(f"Community GUI Edition verified via standard metadata and {png_info['stego_community_gui_count']} transparent pixels")
            return report

        if "Community" in gen or lic.startswith("Community-Exemption-Under-1M"):
            report["verdict"] = ForensicVerdict.COMMUNITY_EXEMPTION
            report["reasons"].append(f"Community metadata present: '{gen}' / '{lic}' (Tool: {report['tool']})")
            if report["tool"] == "CLI":
                report["reasons"].append("NOTE: Automated CI/CD pipeline usage requires a Commercial CLI Automation or Enterprise Studio license.")
            return report

        if "BentoPack" in gen or "BentoPack" in gen or text.get("X-BentoPack-Edition") == "Commercial" or text.get("X-BentoPack-Edition") == "Commercial":
            report["verdict"] = ForensicVerdict.COMMERCIAL_VERIFIED
            report["reasons"].append(f"Authentic clean commercial build metadata and clean transparent pixels (Tool: {report['tool']})")
            return report

    elif ext == ".json":
        json_info = parse_json(file_path, secret_key)
        if not json_info["is_bentopack_json"]:
            return report

        if json_info["tool"]:
            report["tool"] = json_info["tool"]
        elif json_info["app"] and "CLI" in json_info["app"]:
            report["tool"] = "CLI"
        elif json_info["app"] and "GUI" in json_info["app"]:
            report["tool"] = "GUI"

        if json_info["integrity"] == "TAMPERED_CIRCUMVENTION_DETECTED" or json_info["signature"] == "tampered-tamper-detected":
            report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
            report["reasons"].append(f"JSON meta block explicitly flagged TAMPERED_CIRCUMVENTION_DETECTED (Tool: {report['tool']})")
            return report

        if (json_info["signature"] and "unverified" in json_info["signature"]) or (json_info["license"] and "Community" in json_info["license"]):
            report["verdict"] = ForensicVerdict.COMMUNITY_EXEMPTION
            report["reasons"].append(f"JSON descriptor declares Community Edition under 1M$ exemption (Tool: {report['tool']})")
            if report["tool"] == "CLI":
                report["reasons"].append("NOTE: Automated CI/CD pipeline usage requires a Commercial CLI Automation or Enterprise Studio license.")
            return report

        if json_info["signature"] and json_info["signature"].startswith("comm-"):
            if json_info["signature_valid"] is True:
                report["verdict"] = ForensicVerdict.COMMERCIAL_VERIFIED
                report["reasons"].append(f"HMAC-SHA256 signature mathematically verified with master commercial secret key (Tool: {report['tool']})")
            elif json_info["signature_valid"] is False:
                report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
                report["reasons"].append(f"HMAC-SHA256 commercial signature mismatch (counterfeit signature, Tool: {report['tool']})")
            else:
                report["verdict"] = ForensicVerdict.COMMERCIAL_VERIFIED
                report["reasons"].append(f"Commercial signature token format present (Tool: {report['tool']})")
            return report

    elif ext == ".tres":
        tres_info = parse_tres(file_path)
        if not tres_info["is_bentopack"]:
            return report

        if tres_info["tool"]:
            report["tool"] = tres_info["tool"]

        if tres_info["is_tampered"]:
            report["verdict"] = ForensicVerdict.TAMPERED_CIRCUMVENTION
            report["reasons"].append(f"Godot resource comment flags tampered / circumvented build (Tool: {report['tool']})")
            return report

        if tres_info["is_community"]:
            report["verdict"] = ForensicVerdict.COMMUNITY_EXEMPTION
            report["reasons"].append(f"Godot resource comment declares Community Edition under 1M$ exemption (Tool: {report['tool']})")
            if report["tool"] == "CLI":
                report["reasons"].append("NOTE: Automated CI/CD pipeline usage requires a Commercial CLI Automation or Enterprise Studio license.")
            return report

    return report

def main():
    parser = argparse.ArgumentParser(
        description="BentoPack Forensic License & Anti-Tamper Compliance Audit Tool"
    )
    parser.add_argument("target", help="Path to a file or directory of assets to audit")
    parser.add_argument("--key", help="Commercial master secret key to verify HMAC layout signatures", default=None)
    parser.add_argument("--json", action="store_true", help="Output audit results as JSON")
    args = parser.parse_args()

    files_to_check = []
    if os.path.isfile(args.target):
        files_to_check.append(args.target)
    elif os.path.isdir(args.target):
        for root, _, files in os.walk(args.target):
            for f in files:
                ext = os.path.splitext(f)[1].lower()
                if ext in [".png", ".json", ".tres"]:
                    files_to_check.append(os.path.join(root, f))
    else:
        print(f"Error: Target '{args.target}' does not exist.", file=sys.stderr)
        sys.exit(1)

    reports = []
    counts = {
        ForensicVerdict.COMMERCIAL_VERIFIED: 0,
        ForensicVerdict.COMMUNITY_EXEMPTION: 0,
        ForensicVerdict.TAMPERED_CIRCUMVENTION: 0,
        ForensicVerdict.CLEAN_OR_UNKNOWN: 0
    }

    for fpath in files_to_check:
        rep = audit_file(fpath, args.key)
        reports.append(rep)
        counts[rep["verdict"]] += 1

    if args.json:
        print(json.dumps({"summary": counts, "files": reports}, indent=2))
        return

    # Colored human-readable output
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

    print(f"\n{BOLD}=== BentoPack Compliance & Forensic Audit Report ==={RESET}")
    print(f"Target: {args.target}")
    print(f"Total files audited: {len(files_to_check)}\n")

    cli_community_found = False

    for rep in reports:
        v = rep["verdict"]
        tool_label = f" [{rep['tool']}]" if rep['tool'] != "Unknown" else ""
        if v == ForensicVerdict.TAMPERED_CIRCUMVENTION:
            color = RED
            status = f"[TAMPERED / CONTREFAÇON DÉTECTÉE]{tool_label}"
        elif v == ForensicVerdict.COMMUNITY_EXEMPTION:
            color = YELLOW
            status = f"[COMMUNITY EDITION (<1M$ EXEMPTION)]{tool_label}"
            if rep["tool"] == "CLI":
                cli_community_found = True
        elif v == ForensicVerdict.COMMERCIAL_VERIFIED:
            color = GREEN
            status = f"[COMMERCIAL OFFICIEL VALIDE]{tool_label}"
        else:
            continue

        print(f"{color}{BOLD}{status}{RESET} {rep['file']}")
        for r in rep["reasons"]:
            print(f"  └── {r}")

    print(f"\n{BOLD}Audit Summary:{RESET}")
    print(f"  {GREEN}Commercial Verifié :{RESET} {counts[ForensicVerdict.COMMERCIAL_VERIFIED]}")
    print(f"  {YELLOW}Community Exemption:{RESET} {counts[ForensicVerdict.COMMUNITY_EXEMPTION]}")
    print(f"  {RED}Tampered / Piraté  :{RESET} {counts[ForensicVerdict.TAMPERED_CIRCUMVENTION]}")
    print(f"  Autre / Neutre     : {counts[ForensicVerdict.CLEAN_OR_UNKNOWN]}")

    if counts[ForensicVerdict.TAMPERED_CIRCUMVENTION] > 0:
        print(f"\n{RED}{BOLD}ALERTE JURIDIQUE : Contrefaçon intentionnelle caractérisée !{RESET}")
        print("Des traces de contournement délibéré de mesure technique de protection ont été identifiées.")
        sys.exit(2)
    elif cli_community_found:
        print(f"\n{YELLOW}{BOLD}Avertissement CI : Assets générés par la CLI Community identifiés.{RESET}")
        print("Si ces assets ont été produits par un pipeline automatisé ou dans un studio avec CA > 1M$, une licence Commercial CLI Automation ou Enterprise Studio est requise.")
        sys.exit(0)
    elif counts[ForensicVerdict.COMMUNITY_EXEMPTION] > 0:
        print(f"\n{YELLOW}Note : Assets Community identifiés. Valider que le studio réalise < 1,000,000$ de CA annuel.{RESET}")
        sys.exit(0)
    else:
        print(f"\n{GREEN}Tous les assets audités sont conformes.{RESET}")
        sys.exit(0)

if __name__ == "__main__":
    main()
