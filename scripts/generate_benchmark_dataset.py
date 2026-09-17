#!/usr/bin/env python3
"""
generate_benchmark_dataset.py
Generates an exhaustive, multi-tier benchmark dataset for formal validation of spritestudio-cli:
1. characters/         : 4 distinct animated game characters (Knight, Mage, Rogue, Slime) with 48 individual frames.
2. irregular_primes/   : 30 sprites with prime/odd dimensions (e.g. 17x31, 29x53) and extreme aspect ratios (2x120, 160x4).
3. duplicate_cluster/  : 100 frames containing 40 unique frames + 60 exact visual duplicates to test auto-alias deduplication.
4. trim_stress/        : 20 frames (128x128) with small asymmetrical sprites in corners and off-center pivots.
5. raw_sheets/         : Full composite sheets with solid green/magenta backgrounds for slicing & background removal tests.
6. massive_batch/      : 500 procedural pixel art items/icons to benchmark throughput and scalability.
"""

import os
import sys
import math
import random
from PIL import Image, ImageDraw

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATASET_DIR = os.path.join(ROOT_DIR, "benchmarks", "dataset")

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

# ----------------------------------------------------------------------
# 1. Characters with multiple animations (48 individual PNGs)
# ----------------------------------------------------------------------
def generate_characters():
    base_dir = os.path.join(DATASET_DIR, "characters")
    ensure_dir(base_dir)

    # Palette
    C_OUTLINE = (20, 20, 30, 255)
    
    # Archetypes: (Name, PrimaryColor, SecondaryColor, DetailColor)
    archetypes = [
        ("knight", (160, 180, 200, 255), (80, 95, 120, 255), (255, 215, 0, 255)),
        ("mage",   (180, 80, 200, 255),  (90, 40, 110, 255),  (0, 240, 255, 255)),
        ("rogue",  (60, 140, 70, 255),   (35, 75, 40, 255),   (220, 60, 60, 255)),
        ("slime",  (50, 210, 140, 255),  (25, 120, 80, 255),  (255, 255, 255, 255)),
    ]

    for name, c_pri, c_sec, c_det in archetypes:
        char_dir = os.path.join(base_dir, name)
        ensure_dir(char_dir)

        # 12 frames per character: idle (4), walk (4), attack (4)
        for anim in ["idle", "walk", "attack"]:
            for f in range(4):
                img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
                d = ImageDraw.Draw(img)

                if name == "slime":
                    # Slime shape squish/stretch
                    squish_y = 0
                    squish_x = 0
                    if anim == "idle":
                        squish_y = int(math.sin(f * math.pi / 2) * 2)
                    elif anim == "walk":
                        squish_x = (f % 2) * 2
                        squish_y = -((f + 1) % 2) * 2
                    elif anim == "attack":
                        squish_y = 4 if f == 2 else -2

                    bx0, by0 = 8 - squish_x, 14 + squish_y
                    bx1, by1 = 24 + squish_x, 28
                    d.ellipse([bx0, by0, bx1, by1], fill=c_pri, outline=C_OUTLINE)
                    # Core highlight
                    d.ellipse([bx0 + 4, by0 + 3, bx0 + 8, by0 + 6], fill=(255, 255, 255, 200))
                    # Eyes
                    d.rectangle([12 - squish_x, 18 + squish_y, 14 - squish_x, 20 + squish_y], fill=(0, 0, 0, 255))
                    d.rectangle([18 + squish_x, 18 + squish_y, 20 + squish_x, 20 + squish_y], fill=(0, 0, 0, 255))
                else:
                    # Bipedal characters
                    bob = int(math.sin(f * math.pi / 2) * 2) if anim != "attack" else 0
                    # Head
                    d.rectangle([11, 4 + bob, 20, 12 + bob], fill=c_sec, outline=C_OUTLINE)
                    d.rectangle([13, 8 + bob, 18, 9 + bob], fill=c_det)
                    # Body
                    d.rectangle([10, 13 + bob, 21, 21 + bob], fill=c_pri, outline=C_OUTLINE)
                    # Legs
                    l_off = (f - 2) * 2 if anim == "walk" else 0
                    r_off = (2 - f) * 2 if anim == "walk" else 0
                    d.rectangle([11 + l_off, 22, 14 + l_off, 28], fill=c_sec, outline=C_OUTLINE)
                    d.rectangle([17 + r_off, 22, 20 + r_off, 28], fill=c_sec, outline=C_OUTLINE)
                    # Weapon / action
                    if anim == "attack":
                        if f == 2:
                            # Swing
                            d.line([22, 10, 30, 18], fill=c_det, width=2)
                            d.line([28, 12, 31, 26], fill=(255, 255, 255, 200), width=1)
                        else:
                            d.line([22, 16, 26, 22], fill=c_det, width=2)

                frame_path = os.path.join(char_dir, f"{anim}_{f:02d}.png")
                img.save(frame_path, "PNG")

    print(f"[OK] Generated characters (4 archetypes, 48 frames) in {base_dir}")

# ----------------------------------------------------------------------
# 2. Irregular prime dimensions & extreme aspect ratios (30 PNGs)
# ----------------------------------------------------------------------
def generate_irregular_primes():
    out_dir = os.path.join(DATASET_DIR, "irregular_primes")
    ensure_dir(out_dir)

    prime_sizes = [
        (17, 31), (29, 53), (71, 19), (13, 97), (101, 43),
        (37, 67), (19, 83), (47, 59), (23, 79), (89, 31),
        (11, 41), (59, 29), (31, 31), (43, 73), (67, 17),
        (73, 23), (17, 17), (29, 29), (83, 47), (97, 13),
        # Extreme aspect ratios
        (2, 120),   # Vertical beam/laser
        (120, 2),   # Horizontal beam
        (3, 80),    # Thin spear
        (160, 4),   # Boss healthbar
        (4, 160),   # Long pole
        (1, 1),     # Single spark pixel
        (2, 2),     # Tiny particle
        (3, 3),     # 3x3 dot
        (200, 7),   # Wide banner
        (7, 200),   # Tall ladder
    ]

    for idx, (w, h) in enumerate(prime_sizes):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        # Seeded color based on dimensions
        r = (w * 37) % 200 + 40
        g = (h * 43) % 200 + 40
        b = ((w + h) * 19) % 200 + 40
        color = (r, g, b, 255)
        border = (max(0, r - 40), max(0, g - 40), max(0, b - 40), 255)

        if w >= 4 and h >= 4:
            d.rectangle([0, 0, w - 1, h - 1], fill=color, outline=border)
            # Diagonal hatch to make pixel pattern rich
            for diag in range(0, w + h, 8):
                d.line([diag, 0, diag - h, h], fill=(255, 255, 255, 80))
        else:
            d.rectangle([0, 0, w - 1, h - 1], fill=color)

        img.save(os.path.join(out_dir, f"prime_{idx:02d}_{w}x{h}.png"), "PNG")

    print(f"[OK] Generated irregular primes ({len(prime_sizes)} frames) in {out_dir}")

# ----------------------------------------------------------------------
# 3. Duplicate cluster for auto-alias testing (100 PNGs: 40 unique + 60 dups)
# ----------------------------------------------------------------------
def generate_duplicate_cluster():
    out_dir = os.path.join(DATASET_DIR, "duplicate_cluster")
    ensure_dir(out_dir)

    # 40 unique base images (32x32)
    unique_images = []
    for i in range(40):
        img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        # Distinct geometry
        shape_type = i % 4
        c = ((i * 53) % 220 + 30, (i * 79) % 220 + 30, (i * 97) % 220 + 30, 255)
        if shape_type == 0:
            d.rectangle([4, 4, 27, 27], fill=c, outline=(20, 20, 20, 255))
        elif shape_type == 1:
            d.ellipse([4, 4, 27, 27], fill=c, outline=(20, 20, 20, 255))
        elif shape_type == 2:
            d.polygon([(16, 4), (28, 26), (4, 26)], fill=c, outline=(20, 20, 20, 255))
        else:
            d.rectangle([8, 8, 23, 23], fill=c, outline=(20, 20, 20, 255))
            d.ellipse([12, 12, 19, 19], fill=(255, 255, 255, 255))

        # Distinct label text or small detail pixel
        d.point([16, 16], fill=(0, 0, 0, 255))
        d.point([15, 16], fill=(255, 255, 255, 255))
        unique_images.append(img)

    # 100 outputs total:
    # First 40: unique_00 to unique_39
    for i in range(40):
        unique_images[i].save(os.path.join(out_dir, f"unique_{i:02d}.png"), "PNG")

    # Remaining 60: duplicates mapped cyclically into the 40 unique images
    for i in range(60):
        source_idx = i % 40
        unique_images[source_idx].save(os.path.join(out_dir, f"dup_{i:02d}_matches_{source_idx:02d}.png"), "PNG")

    print(f"[OK] Generated duplicate cluster (100 frames: 40 unique + 60 duplicates) in {out_dir}")

# ----------------------------------------------------------------------
# 4. Trim stress & asymmetrical pivots (20 PNGs)
# ----------------------------------------------------------------------
def generate_trim_stress():
    out_dir = os.path.join(DATASET_DIR, "trim_stress")
    ensure_dir(out_dir)

    for i in range(20):
        # 128x128 canvas with small 24x32 sprite positioned in various corners
        img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        # Positions: top-left, bottom-right, top-right, center, etc.
        pos_mode = i % 5
        if pos_mode == 0:
            x0, y0 = 6, 6
        elif pos_mode == 1:
            x0, y0 = 96, 88
        elif pos_mode == 2:
            x0, y0 = 98, 6
        elif pos_mode == 3:
            x0, y0 = 6, 90
        else:
            x0, y0 = 52, 48

        # Draw a little gem / character
        color = ((i * 45) % 200 + 50, (i * 85) % 200 + 50, 240, 255)
        d.rectangle([x0, y0, x0 + 23, y0 + 31], fill=color, outline=(20, 20, 30, 255))
        d.ellipse([x0 + 6, y0 + 6, x0 + 17, y0 + 17], fill=(255, 255, 255, 220))

        img.save(os.path.join(out_dir, f"padded_frame_{i:02d}.png"), "PNG")

    print(f"[OK] Generated trim stress dataset (20 frames, 128x128 padded) in {out_dir}")

# ----------------------------------------------------------------------
# 5. Raw composite sheets with solid background for slicing (2 PNGs)
# ----------------------------------------------------------------------
def generate_raw_sheets():
    out_dir = os.path.join(DATASET_DIR, "raw_sheets")
    ensure_dir(out_dir)

    # 1. Sheet on solid Green #00FF00
    sheet_green = Image.new("RGBA", (256, 256), (0, 255, 0, 255))
    d_g = ImageDraw.Draw(sheet_green)
    # Draw a 4x4 grid of distinct 32x32 sprites separated by 16px
    for row in range(4):
        for col in range(4):
            x = 16 + col * 56
            y = 16 + row * 56
            # Sprite body (non-green)
            col_body = (220, 60 + row * 40, 60 + col * 40, 255)
            d_g.rectangle([x, y, x + 35, y + 35], fill=col_body, outline=(30, 30, 30, 255))
            d_g.ellipse([x + 8, y + 8, x + 27, y + 27], fill=(255, 220, 0, 255))

    sheet_green.save(os.path.join(out_dir, "sheet_green_bg.png"), "PNG")

    # 2. Sheet on solid Magenta #FF00FF
    sheet_mag = Image.new("RGBA", (256, 256), (255, 0, 255, 255))
    d_m = ImageDraw.Draw(sheet_mag)
    for row in range(4):
        for col in range(4):
            x = 20 + col * 54
            y = 20 + row * 54
            col_body = (60 + col * 40, 180, 80 + row * 35, 255)
            d_m.rectangle([x, y, x + 31, y + 31], fill=col_body, outline=(10, 10, 10, 255))

    sheet_mag.save(os.path.join(out_dir, "sheet_magenta_bg.png"), "PNG")

    # 3. Sheet on transparent background (for standard alpha-based slice)
    sheet_trans = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    d_t = ImageDraw.Draw(sheet_trans)
    for row in range(4):
        for col in range(4):
            x = 16 + col * 56
            y = 16 + row * 56
            col_body = (200, 100 + row * 30, 80 + col * 35, 255)
            d_t.rectangle([x, y, x + 35, y + 35], fill=col_body, outline=(30, 30, 30, 255))
            d_t.ellipse([x + 8, y + 8, x + 27, y + 27], fill=(255, 255, 0, 255))

    sheet_trans.save(os.path.join(out_dir, "sheet_transparent_bg.png"), "PNG")

    print(f"[OK] Generated raw sheets (3 composite sheets: green, magenta, transparent) in {out_dir}")

# ----------------------------------------------------------------------
# 6. Massive batch for scalability stress-testing (500 PNGs)
# ----------------------------------------------------------------------
def generate_massive_batch():
    out_dir = os.path.join(DATASET_DIR, "massive_batch")
    ensure_dir(out_dir)

    random.seed(42)  # Deterministic generation
    sizes = [16, 24, 32, 48]

    for i in range(500):
        sz = sizes[i % len(sizes)]
        img = Image.new("RGBA", (sz, sz), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        # Random shape: sword, potion, coin, rune, shield
        t = i % 5
        r, g, b = random.randint(40, 240), random.randint(40, 240), random.randint(40, 240)
        c = (r, g, b, 255)
        border = (max(0, r - 40), max(0, g - 40), max(0, b - 40), 255)

        m = 2
        if t == 0:  # Coin / Circle
            d.ellipse([m, m, sz - 1 - m, sz - 1 - m], fill=c, outline=border)
        elif t == 1:  # Shield / Polygon
            pts = [(sz // 2, m), (sz - 1 - m, m + 4), (sz - 1 - m, sz // 2), (sz // 2, sz - 1 - m), (m, sz // 2), (m, m + 4)]
            d.polygon(pts, fill=c, outline=border)
        elif t == 2:  # Potion / Flask
            d.rectangle([sz // 2 - 2, m, sz // 2 + 2, m + 4], fill=(180, 180, 180, 255))
            d.ellipse([m + 2, m + 5, sz - 1 - m - 2, sz - 1 - m], fill=c, outline=border)
        elif t == 3:  # Gem / Diamond
            mid = sz // 2
            d.polygon([(mid, m), (sz - 1 - m, mid), (mid, sz - 1 - m), (m, mid)], fill=c, outline=border)
        else:  # Box / Tile
            d.rectangle([m, m, sz - 1 - m, sz - 1 - m], fill=c, outline=border)

        img.save(os.path.join(out_dir, f"item_{i:03d}_{sz}x{sz}.png"), "PNG")

    print(f"[OK] Generated massive batch (500 procedural items) in {out_dir}")

def main():
    print("=" * 70)
    print("Generating Comprehensive Benchmark Dataset for SpriteStudio CLI...")
    print("=" * 70)
    ensure_dir(DATASET_DIR)
    generate_characters()
    generate_irregular_primes()
    generate_duplicate_cluster()
    generate_trim_stress()
    generate_raw_sheets()
    generate_massive_batch()
    print("=" * 70)
    print(f"Dataset successfully created in: {DATASET_DIR}")
    print("=" * 70)

if __name__ == "__main__":
    main()
