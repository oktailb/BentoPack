import os

palettes_dir = r"c:\Users\ec135\Documents\GitHub\SpriteStudio\palettes"

# 1. Sega Master System (64 colors - 6-bit hardware master palette)
# 4 levels per channel: 0, 85, 170, 255
sms_colors = []
levels = [0, 85, 170, 255]
for r in levels:
    for g in levels:
        for b in levels:
            sms_colors.append((r, g, b))

# Sort visually by hue / brightness for artist convenience
def color_sort_key(c):
    r, g, b = c
    # black first, white last, then hue/lum
    brightness = 0.299 * r + 0.587 * g + 0.114 * b
    if r == g == b:
        return (0, brightness, 0, 0)
    # simple hue approximation
    import colorsys
    h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
    return (1, round(h * 8), brightness, s)

sms_colors.sort(key=color_sort_key)

# 2. Sega Mega Drive (64 colors - 9-bit RGB quantized steps: 0, 36, 73, 109, 146, 182, 219, 255)
# Hand-curated iconic 64-color Mega Drive palette
# 4 sub-palettes of 16 colors matching Sonic, Streets of Rage, Shinobi, Golden Axe
smd_colors = [
    # Grayscale & Monochrome (8)
    (0, 0, 0), (36, 36, 36), (73, 73, 73), (109, 109, 109),
    (146, 146, 146), (182, 182, 182), (219, 219, 219), (255, 255, 255),
    # Sonic Blue ramp (6)
    (0, 0, 73), (0, 0, 146), (0, 36, 219), (0, 73, 255), (73, 146, 255), (146, 219, 255),
    # Sky / Cyan ramp (5)
    (0, 73, 109), (0, 146, 182), (0, 219, 219), (73, 255, 255), (182, 255, 255),
    # Green Hill Foliage & Emeralds (7)
    (0, 73, 0), (0, 109, 0), (0, 146, 0), (0, 219, 0), (73, 255, 0), (146, 255, 73), (219, 255, 146),
    # Tails Gold & Yellow ramp (6)
    (109, 73, 0), (182, 109, 0), (219, 146, 0), (255, 182, 0), (255, 219, 0), (255, 255, 109),
    # Earth, Wood & Stone (6)
    (73, 36, 0), (109, 73, 36), (146, 73, 36), (182, 109, 73), (219, 146, 109), (255, 219, 182),
    # Knuckles Crimson & Red ramp (6)
    (73, 0, 0), (146, 0, 0), (219, 0, 0), (255, 36, 0), (255, 73, 73), (255, 146, 146),
    # Streets of Rage Neon Violet & Magenta (6)
    (73, 0, 73), (146, 0, 146), (219, 0, 182), (255, 36, 219), (255, 109, 255), (255, 182, 255),
    # Deep Navy & Night (4)
    (0, 36, 73), (36, 73, 109), (73, 109, 146), (109, 146, 182),
    # Dark Olive & Military (4)
    (36, 73, 36), (73, 109, 73), (109, 146, 109), (146, 182, 146),
    # Sunset Amber & Coral (6)
    (146, 36, 0), (219, 73, 0), (255, 109, 36), (255, 146, 73), (255, 182, 109), (255, 219, 146)
]
assert len(smd_colors) == 64

# 3. Sega Saturn / 32-bit (64 colors - 15-bit RGB high-fidelity arcade ramps)
# Panzer Dragoon, Guardian Heroes, Virtua Fighter, Castlevania SOTN style
saturn_colors = [
    # Monochromes (6)
    (0, 0, 0), (40, 40, 48), (88, 88, 96), (144, 144, 152), (200, 200, 208), (255, 255, 255),
    # Cool Slate & Steel (5)
    (24, 32, 48), (48, 64, 88), (80, 104, 136), (128, 152, 184), (184, 208, 232),
    # Deep Ocean & Electric Blue (5)
    (16, 24, 72), (24, 48, 128), (40, 88, 200), (72, 136, 248), (136, 184, 255),
    # Cyan & Lapis Sky (5)
    (8, 56, 80), (16, 96, 128), (32, 152, 192), (72, 208, 240), (160, 240, 255),
    # Emerald & Jade Forest (5)
    (8, 48, 32), (16, 88, 56), (32, 144, 88), (64, 208, 128), (144, 248, 184),
    # Toxic Lime & Yellow-Green (5)
    (32, 56, 16), (64, 104, 24), (112, 168, 40), (168, 224, 56), (216, 255, 112),
    # Solar Gold & Amber (5)
    (64, 40, 8), (128, 80, 16), (200, 136, 24), (248, 184, 48), (255, 232, 112),
    # Terracotta & Warm Wood (5)
    (48, 24, 16), (88, 44, 28), (144, 76, 48), (196, 116, 76), (236, 164, 124),
    # Blood Crimson & Ruby (5)
    (56, 12, 20), (112, 24, 36), (184, 40, 56), (236, 72, 88), (255, 136, 148),
    # Royal Violet & Amethyst (5)
    (40, 16, 56), (80, 32, 104), (136, 56, 168), (192, 96, 224), (232, 160, 248),
    # Neon Magenta & Cyber Pink (5)
    (56, 8, 40), (112, 20, 80), (184, 36, 128), (240, 72, 176), (255, 144, 216),
    # Warm Skin Tones (4)
    (112, 64, 52), (176, 112, 88), (232, 168, 136), (255, 216, 192),
    # Antique Brass & Bronze (4)
    (72, 56, 32), (120, 96, 56), (176, 144, 88), (224, 196, 136)
]
assert len(saturn_colors) == 64

# 4. SNK Neo Geo (64 colors - MVS/AES arcade punchy comic/pixel aesthetic)
# Metal Slug military + KOF fighting + Fatal Fury vibrant palette
neogeo_colors = [
    # True Arcade Black & Grays (6)
    (0, 0, 0), (32, 32, 36), (72, 72, 80), (120, 120, 132), (180, 180, 192), (248, 248, 252),
    # Metal Slug Heavy Steel & Armor (5)
    (24, 28, 40), (44, 56, 76), (72, 88, 116), (112, 136, 168), (164, 188, 216),
    # Metal Slug Camo Olive & Khaki (5)
    (32, 40, 20), (56, 68, 36), (88, 104, 56), (132, 148, 88), (180, 196, 128),
    # Deep Jungle Green (5)
    (12, 36, 24), (20, 68, 44), (32, 112, 68), (56, 168, 100), (112, 224, 152),
    # Fighting Stage Deep Cyan / Teal (5)
    (8, 44, 56), (16, 80, 96), (28, 128, 148), (52, 180, 204), (124, 228, 244),
    # Terry Bogard / Kyo Kusanagi Blue (5)
    (16, 20, 64), (28, 44, 120), (44, 76, 196), (76, 124, 248), (144, 180, 255),
    # Fatal Fury Crimson & Red (5)
    (64, 12, 16), (124, 20, 28), (196, 32, 40), (248, 64, 60), (255, 136, 128),
    # Metal Slug Fire Explosion & Orange (5)
    (80, 28, 8), (148, 56, 12), (220, 96, 20), (252, 148, 36), (255, 204, 76),
    # Neo Geo Arcade Gold & Yellow (5)
    (72, 52, 8), (136, 100, 16), (208, 160, 28), (252, 212, 56), (255, 244, 136),
    # Iori Yagami Purple & Violet (5)
    (36, 16, 52), (72, 32, 100), (124, 56, 164), (180, 92, 228), (224, 156, 252),
    # Arcade Neon Pink / Magenta (5)
    (64, 12, 44), (124, 24, 84), (196, 40, 132), (248, 76, 180), (255, 148, 216),
    # Character Skin Tones (4)
    (96, 52, 40), (160, 96, 72), (224, 152, 116), (252, 208, 176),
    # Tanned / Muscle Shadow Tones (4)
    (68, 36, 28), (128, 72, 52), (188, 120, 88), (236, 176, 140)
]
assert len(neogeo_colors) == 64

def write_gpl(filename, name, colors):
    filepath = os.path.join(palettes_dir, filename)
    with open(filepath, "w", encoding="utf-8") as f:
        f.write("GIMP Palette\n")
        f.write(f"Name: {name}\n")
        f.write("Columns: 8\n#\n")
        for r, g, b in colors:
            hex_str = f"#{r:02x}{g:02x}{b:02x}"
            f.write(f"{r:3d} {g:3d} {b:3d}\t{hex_str}\n")
    print(f"Wrote {filepath} ({len(colors)} colors)")

write_gpl("sega_master_system.gpl", "Sega Master System (64)", sms_colors)
write_gpl("sega_megadrive.gpl", "Sega Mega Drive / Genesis (64)", smd_colors)
write_gpl("sega_saturn.gpl", "Sega Saturn / 32-bit (64)", saturn_colors)
write_gpl("neogeo.gpl", "SNK Neo Geo (64)", neogeo_colors)
