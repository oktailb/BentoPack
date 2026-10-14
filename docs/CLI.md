# BentoPack CLI (`bentopack-cli`)
> **Headless 2D Texture Atlas Packing, Smart Slicing, Image Processing, and Game Engine Asset Compiler**

`bentopack-cli` is the command-line counterpart of BentoPack. Built with modern C++17 and Qt 6 headless primitives (`QT_QPA_PLATFORM=offscreen`), it operates without a display server or windowing environment (no X11, Wayland, or desktop session required).

It serves three key workflows:
1. **Drop-in Replacement**: Seamless 1:1 replacement for **TexturePacker** and **Aseprite** in existing build scripts and Makefiles.
2. **Game Engine Asset Pipelines**: Native generation of production-ready resources for **Godot 4** (`.tres`, `.tscn`), **Unity 2D** (`.unity.json`, Sprite Meshes), and **Unreal Engine** (`Paper2D`).
3. **AI Coding Agents & CI/CD**: Machine-readable JSON output (`--json`), self-describing introspection schema (`--schema`), POSIX exit codes, and an isolated background watch daemon (`--watch --daemon`).

---

## Table of Contents
- [1. Quick Start & Flavors](#1-quick-start--flavors)
- [2. Subcommands & Capabilities](#2-subcommands--capabilities)
  - [2.1. `pack` - Sprite Atlas Compilation](#21-pack---sprite-atlas-compilation)
  - [2.2. `slice` - Smart Sprite Detection & Background Removal](#22-slice---smart-sprite-detection--background-removal)
  - [2.3. `filter` - Headless Image Processing](#23-filter---headless-image-processing)
  - [2.4. `bento` (or `ssp`) - Project Inspection & Revision Management](#24-bento-or-ssp---project-inspection--revision-management)
  - [2.5. `watch` & `daemon` - Background Live-Reloading](#25-watch--daemon---background-live-reloading)
  - [2.6. `schema` - AI Agent Introspection](#26-schema---ai-agent-introspection)
- [3. BentoPack Unique Superpowers](#3-bentopack-unique-superpowers)
  - [Multi-Page Atlas & Spanning (M16)](#multi-page-atlas--spanning-m16)
  - [GPU VRAM Texture Compression (KTX2 / Basis Universal)](#gpu-vram-texture-compression-ktx2--basis-universal)
  - [M8 Tight Polygonal Mesh Packing](#m8-tight-polygonal-mesh-packing)
- [4. AI Agent Integration Guide](#4-ai-agent-integration-guide)
  - [JSON Output Format](#json-output-format)
  - [POSIX Exit Codes](#posix-exit-codes)
  - [Python Tool Definition Example](#python-tool-definition-example)
- [5. Production Real-World Examples](#5-production-real-world-examples)

---

## 1. Quick Start & Flavors

`bentopack-cli` automatically determines the expected syntax flavor based on `argv[0]` or the `--flavor` flag:

```bash
# 1. Native BentoPack Pack
bentopack-cli pack --sheet atlas.png --data atlas.json sprites/

# 2. TexturePacker Drop-in (symlink or alias)
TexturePacker --sheet atlas.png --data atlas.json --format json-array --trim-mode Trim sprites/

# 3. Aseprite Batch Mode
aseprite -b character.aseprite --sheet char_sheet.png --data char_sheet.json --list-tags

# 4. Force syntax flavor explicitly
bentopack-cli --flavor tp --sheet atlas.png --data atlas.json sprites/
```

### Global Options

| Flag | Description |
| :--- | :--- |
| `-h`, `--help` | Display command-line syntax and option summary |
| `-v`, `--version` | Display BentoPack version and build details |
| `--json` | Force clean JSON output to standard output (all logs redirected to stderr or suppressed) |
| `--schema` | Output comprehensive machine-readable JSON Schema for AI agent tool-calling |
| `--quiet` | Suppress non-essential progress and status messages |
| `--verbose` | Output verbose engine details (heuristics, cluster allocation, compression stats) |
| `--flavor <tp\|aseprite\|godot\|native>` | Force CLI syntax parser flavor |

---

## 2. Subcommands & Capabilities

### 2.1. `pack` - Sprite Atlas Compilation

Packs loose images (`.png`, `.jpg`, `.webp`, `.bmp`), Aseprite files (`.ase`, `.aseprite`), or native BentoPack archives (`.bento`) into an optimized texture atlas with corresponding game engine metadata.

```bash
bentopack-cli pack [options] <input_files_or_directories...>
```

#### Packing Parameters

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `--sheet <file>` | Path | *(required)* | Destination image path (`.png`, `.webp`, `.ktx2`, `.basis`). |
| `--data <file>` | Path | `""` | Destination metadata path (`.json`, `.tres`, `.unity.json`). |
| `--format <fmt>` | String | Auto | Output metadata format: `json-array`, `json-hash`, `godot4`, `unity`, `unreal`, `libgdx`, `spine`, `bento`. |
| `--algorithm <algo>` | String | `MaxRects` | Packing algorithm: `MaxRects`, `Basic` (RowPacker), `Grid`, `TightPolygon`. |
| `--maxrects-heuristics <h>` | String | `BestShortSideFit` | MaxRects rule: `BestShortSideFit`, `BestLongSideFit`, `BestAreaFit`, `BottomLeft`, `ContactPoint`. |
| `--size-constraints <c>` | String | `AnySize` | Constraint: `AnySize` (arbitrary rectangle) or `POT` (Power of Two). |
| `--max-size <w> <h>` | Int Int | `4096 4096` | Maximum width and height bounds for an atlas sheet. |
| `--padding <px>` | Int | `2` | Pixel spacing between adjacent sprite rectangles. |
| `--border-padding <px>` | Int | `0` | Margin around the outer edge of the texture atlas. |
| `--extrude <px>` | Int | `0` | Replicate border pixels outward to prevent texture filtering discoloration / bleeding. |
| `--trim-mode <mode>` | String | `None` | Transparent pixel trimming: `None`, `Trim`, or `Crop`. |
| `--trim-threshold <0-255>` | Int | `1` | Alpha value below which pixels are considered transparent. |
| `--enable-auto-alias` | Flag | Off | Deduplicate visually identical sprites using cryptographic/pixel hashing. |
| `--pivot-point <x> <y>` | Float Float | `0.5 0.5` | Normalized anchor point `[0.0 - 1.0]` (e.g. `0.5 1.0` for character feet). |
| `--godot-uid <uid>` | String | `""` | Explicit Godot 4 stable UID to embed in `.tres`. |
| `--godot-scene <file.tscn>`| Path | `""` | Automatically generate ready-to-instantiate Godot 4 `AnimatedSprite2D` scene. |
| `--multipage` | Flag | Off | Enable multi-page atlas spanning (M16) when sprites exceed maximum size bounds. |
| `--texture-format <fmt>` | String | `png` | Texture encoder: `png`, `webp`, `ktx2`, `basis`. |
| `--vram-format <codec>` | String | `uastc` | GPU codec for KTX2/Basis: `uastc` (high fidelity) or `etc1s` (ultra compact). |
| `--vram-quality <0-255>` | Int | `128` | Encoder quality level for VRAM compression. |
| `--enable-zstd` / `--disable-zstd` | Flag | Enabled | Toggle Zstandard supercompression on KTX2/Basis files. |
| `--zstd-level <1-22>` | Int | `9` | Zstandard compression level. |

---

### 2.2. `slice` - Smart Sprite Detection & Background Removal

Analyzes composite raw sprite sheets, tilesets, or scans, automatically detecting individual sprite boundaries using connected-component analysis and boundary discovery.

```bash
bentopack-cli slice <input_image> [options]
```

#### Slicing Options

| Option | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `--output-dir <dir>` | Path | `""` | Directory where segmented sprite frames are saved (`frame_0000.png`...). |
| `--output-project <file.bento>`| Path | `""` | Save extracted sprites directly into a native `.bento` project archive. |
| `--remove-bg` | Flag | Off | Automatically identify and erase uniform background color (chroma-keying). |
| `--tolerance <0-100>` | Int | `10` | Color Euclidean distance tolerance for background removal. |
| `--alpha-threshold <0-255>` | Int | `1` | Alpha cutoff for connected-component boundary detection. |
| `--smart-crop` | Flag | Off | Snugly shrink detected bounding boxes to the exact non-transparent pixel boundary. |
| `--min-size <px>` | Int | `3` | Minimum width and height threshold to filter out noise and dust specks. |

---

### 2.3. `filter` - Headless Image Processing

Applies deterministic image transformations and corrections to sprites without opening a graphical interface.

```bash
bentopack-cli filter <input_image> [filter_options] --output <output_image>
```

#### Filter Operations

| Option | Description | Example |
| :--- | :--- | :--- |
| `--despill [hex_color]` | Remove color fringe halos around segmented sprites | `--despill #00FF00` |
| `--despill-mode <clamp\|strict>` | Despill algorithm aggression | `--despill-mode clamp` |
| `--outline [thickness_px]` | Add an external contour stroke around sprite silhouettes | `--outline 2` |
| `--outline-color <#hex>` | Stroke color for outline | `--outline-color #000000` |
| `--silhouette` | Convert sprite into solid color mask matching outline color | `--silhouette` |
| `--color-swap <#src:#dst>` | Replace a specific palette color while preserving shading | `--color-swap #FF0000:#0000FF` |
| `--color-adjust` | Adjust HSL values and contrast | `--color-adjust --hue 45 --contrast 120` |
| `--pixel-rescale <2x\|3x\|4x>` | Scale pixel art using Nearest-Neighbor interpolation | `--pixel-rescale 2x` |

---

### 2.4. `bento` (or `ssp`) - Project Inspection & Revision Management

Interacts with native `.bento` ZIP container archives.

```bash
# 1. Inspect project metadata in JSON
bentopack-cli bento project.bento --info --json

# 2. Headless export to engine format
bentopack-cli bento project.bento --export godot4 --output export/player.tres

# 3. Checkout revision from embedded Git version control (LibGit2)
bentopack-cli bento project.bento --checkout-revision a1b2c3d4 --export unity --output export/player.unity.json
```

---

### 2.5. `watch` & `daemon` - Background Live-Reloading

Monitors source folders in real-time. When an artist saves an image, `bentopack-cli` automatically repacks the atlas and updates the engine descriptor.

```bash
# 1. Interactive terminal watch
bentopack-cli pack --sheet atlas.png --data atlas.tres --format godot4 --watch assets/sprites/

# 2. Background daemon guarded by atomic directory .lock
bentopack-cli pack --sheet atlas.png --data atlas.tres --format godot4 --watch --daemon --debounce 250 assets/sprites/

# 3. Terminate background daemon gracefully
bentopack-cli --stop-watch assets/sprites/
```

- **Loop Protection**: Target atlas files (`--sheet` and `--data`) are automatically filtered out from watcher events to prevent infinite rebuild loops.
- **Atomic Locking**: Writes a `.bentopack.lock` file containing the PID, timestamp, and command. Exits with code `5` (`ExitLockConflict`) if a watcher is already active for the directory.

---

### 2.6. `schema` - AI Agent Introspection

Designed specifically for autonomous AI agents and language models (LLMs). When called with `--schema` or `schema`, the CLI outputs a complete JSON manifest describing all supported subcommands, options, algorithms, formats, and exit codes.

```bash
bentopack-cli --schema
```

---

## 3. BentoPack Unique Superpowers

### Multi-Page Atlas & Spanning (M16)

When exporting thousands of high-resolution sprites or multi-frame animations, assets frequently exceed maximum GPU hardware limits (e.g. 2048x2048 or 4096x4096).

Adding `--multipage` triggers the **MultiPagePacker** engine:
- **Semantic Animation Cohesion**: Frames of the same animation cycle are guaranteed to stay on the same page, preventing GPU state-swapping during runtime playback.
- **Deterministic Numbering**: Emits `sheet_0.png`, `sheet_1.png`... (or `.ktx2`).
- **Engine Coordination**:
  - **Godot 4**: Automatically declares multiple `CanvasTexture_page_0`, `CanvasTexture_page_1` and binds each `AtlasTexture` to its respective page.
  - **Unity 2D**: Populates the `pages` array and tags each sprite with its `page` index.

```bash
bentopack-cli pack \
    --sheet build/atlas.png \
    --data build/atlas.tres \
    --format godot4 \
    --max-size 2048 2048 \
    --multipage \
    raw_character_frames/
```

### GPU VRAM Texture Compression (KTX2 / Basis Universal)

BentoPack includes native Basis Universal encoders. Rather than storing standard PNG files that must be decompressed uncompressed into GPU memory (consuming 4 bytes per pixel), `bentopack-cli` compiles directly into GPU-transcodable textures:

```bash
bentopack-cli pack \
    --sheet dist/atlas.ktx2 \
    --data dist/atlas.json \
    --format json-array \
    --vram-format uastc \
    --enable-zstd \
    --zstd-level 12 \
    sprites/
```
- **UASTC**: Visually lossless for crisp 2D pixel art and PBR auxiliary maps.
- **ETC1S**: Extreme compression (often 85-90% smaller than PNG).
- **Zstandard**: Applied transparently as a supercompression layer.

### M8 Tight Polygonal Mesh Packing

Reduces overdraw on transparent sprite borders by packing polygonal contours rather than simple bounding boxes:

```bash
bentopack-cli pack \
    --sheet atlas.png \
    --data atlas.unity.json \
    --format unity \
    --algorithm TightPolygon \
    sprites/
```

---

## 4. AI Agent Integration Guide

`bentopack-cli` is architected for zero-friction integration with autonomous AI coding agents (Claude Computer Use, Gemini Function Calling, OpenAI Assistants).

### JSON Output Format

When passed `--json`, standard output is guaranteed to be a single, valid JSON document:

#### Success Response
```json
{
  "status": "success",
  "atlas": "build/atlas.png",
  "data": "build/atlas.tres",
  "width": 1024,
  "height": 512,
  "frames_count": 64,
  "unique_frames": 48,
  "efficiency": 94.2,
  "elapsed_ms": 32,
  "is_multipage": false
}
```

#### Multi-Page Success Response
```json
{
  "status": "success",
  "is_multipage": true,
  "pages_count": 2,
  "pages": [
    { "index": 0, "sheet": "build/atlas_0.png", "width": 2048, "height": 2048, "efficiency": 92.1 },
    { "index": 1, "sheet": "build/atlas_1.png", "width": 1024, "height": 1024, "efficiency": 88.4 }
  ],
  "atlas": "build/atlas_0.png",
  "data": "build/atlas.tres",
  "frames_count": 250,
  "unique_frames": 250,
  "elapsed_ms": 78
}
```

#### Error Response
```json
{
  "status": "error",
  "error_code": 3,
  "error_message": "Packing failed: frames exceed max-size constraints (2048x2048). Tip: Use --multipage to span across multiple atlas pages."
}
```

### POSIX Exit Codes

| Code | Constant | Meaning | Recommended Agent Action |
| :---: | :--- | :--- | :--- |
| `0` | `ExitSuccess` | Completed successfully. | Proceed to next pipeline step. |
| `1` | `ExitSyntaxError` | Unknown flag or missing arguments. | Call `bentopack-cli --schema` to verify flag syntax. |
| `2` | `ExitFileNotFound` | Input path does not exist. | Verify source file paths on disk. |
| `3` | `ExitConstraintFailed`| Sprites exceed max texture dimensions. | Add `--multipage` or increase `--max-size`. |
| `4` | `ExitIoError` | Disk write permission error or bad path. | Verify directory write permissions / create directories. |
| `5` | `ExitLockConflict` | Watch daemon already active. | Run `bentopack-cli --stop-watch <dir>` or choose another folder. |
| `6` | `ExitPluginNotFound`| Unknown exporter or encoder plugin. | Check plugin folder or verify format string. |

---

### Python Tool Definition Example

Here is a ready-to-use Python module for AI agent orchestrators:

```python
import subprocess
import json
from typing import Dict, Any, List, Optional

def call_bentopack_cli(args: List[str]) -> Dict[str, Any]:
    """
    Executes bentopack-cli with guaranteed structured JSON output.
    """
    cmd = ["bentopack-cli"] + args + ["--json"]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {
            "status": "error",
            "error_code": proc.returncode,
            "error_message": proc.stderr.strip() or f"Process exited with code {proc.returncode}"
        }
    
    data["exit_code"] = proc.returncode
    return data

# Example Agent Execution
if __name__ == "__main__":
    # 1. Discover capabilities dynamically
    schema = call_bentopack_cli(["--schema"])
    print(f"BentoPack CLI v{schema.get('version')} initialized.")

    # 2. Pack an asset folder into a Godot 4 resource with auto-aliasing
    result = call_bentopack_cli([
        "pack",
        "--sheet", "game/assets/player.png",
        "--data", "game/assets/player.tres",
        "--format", "godot4",
        "--padding", "2",
        "--enable-auto-alias",
        "--trim-mode", "Trim",
        "raw_assets/player/"
    ])

    if result["status"] == "success":
        print(f"Atlas generated: {result['atlas']} ({result['width']}x{result['height']}) in {result['elapsed_ms']}ms")
    else:
        print(f"Agent Error ({result['error_code']}): {result['error_message']}")
```

---

## 5. Production Real-World Examples

### 1. Full Godot 4 CI/CD Asset Pipeline
Compiles all character frames, exports SpriteFrames `.tres`, preserves engine UID, and generates an instantiable `.tscn` scene:

```bash
bentopack-cli pack \
    --sheet res://assets/characters/knight.png \
    --data res://assets/characters/knight.tres \
    --format godot4 \
    --godot-scene res://scenes/characters/Knight.tscn \
    --godot-uid "uid://bento_knight_42" \
    --padding 2 \
    --trim-mode Trim \
    --enable-auto-alias \
    --pivot-point 0.5 1.0 \
    assets_raw/knight/
```

### 2. Unity 2D Pipeline with Tight Mesh & Multi-Page
Packs complex sprites across pages with tight meshes and automatic pixel extrude:

```bash
bentopack-cli pack \
    --sheet Assets/Sprites/Enemies/Boss.png \
    --data Assets/Sprites/Enemies/Boss.unity.json \
    --format unity \
    --algorithm TightPolygon \
    --extrude 1 \
    --max-size 2048 2048 \
    --multipage \
    raw_boss_frames/
```

### 3. Raw Sprite Sheet Auto-Slicing & Extraction
Slices a downloaded raw image sheet with green-screen background removal:

```bash
bentopack-cli slice retro_sheet.png \
    --remove-bg \
    --tolerance 15 \
    --smart-crop \
    --min-size 8 \
    --output-dir output_frames/
```

### 4. Sprite Post-Processing (Outline & Despill)
Removes background fringe bleed and adds a clean 1-pixel outer stroke:

```bash
bentopack-cli filter hero_frame.png \
    --despill \
    --outline 1 \
    --outline-color #1a1a1a \
    --output hero_frame_clean.png
```

### 5. Automated CI GitHub Actions Workflow
```yaml
name: Compile Game Sprites
on: [push]
jobs:
  assets:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install BentoPack CLI
        run: sudo apt-get update && sudo apt-get install -y bentopack-cli
      - name: Build Texture Atlases
        run: |
          bentopack-cli pack \
            --sheet assets/atlas.png \
            --data assets/atlas.tres \
            --format godot4 \
            --enable-auto-alias \
            --json \
            src_sprites/
```

---

*Copyright (c) 2026 Vincent LECOQ. Licensed under the Apache License, Version 2.0.*
