# BentoPack Color Palettes

This directory contains standard retro and studio color palettes for **BentoPack**.

## Formats Supportés
- **`.gpl` (GIMP / Aseprite Palette)** : Format standard contenant l'en-tête `GIMP Palette`, le nom et les triplets RGB.
- **`.hex` (Lospec Palette)** : Un code couleur hexadécimal par ligne (ex: `#1a1c2c` ou `1a1c2c`).
- **`.pal` (JASC Paint Shop Pro / Aseprite)** : Format RGB avec en-tête `JASC-PAL`.

## Palettes Standards Incluses
1. `bento_standard.gpl` — Palette Bento Studio 36 couleurs équilibrées
2. `gameboy_dmg.gpl` — Game Boy Original DMG (4 verts)
3. `gameboy_pocket.gpl` — Game Boy Pocket / Light (4 vrais gris)
4. `nes.gpl` — Nintendo Entertainment System / Famicom (54 couleurs)
5. `snes.gpl` — Super Nintendo / 16-bit (32 couleurs)
6. `pico8.gpl` — PICO-8 Fantasy Console (16 couleurs)
7. `c64.gpl` — Commodore 64 (16 couleurs)
8. `amiga.gpl` — Amiga OCS (32 couleurs)
9. `pc_engine.gpl` — NEC PC-Engine / TurboGrafx-16 (32 couleurs)
10. `cga_mode1.gpl` — IBM CGA Mode 1 (Noir, Cyan, Magenta, Blanc)
11. `cga_mode2.gpl` — IBM CGA Mode 2 (Noir, Vert, Rouge, Jaune)
12. `endesga32.gpl` — EDG 32 par Endesga (32 couleurs pixel art)

## Ajout de palettes personnalisées
Pour ajouter vos propres palettes :
- Placez simplement vos fichiers `.gpl` ou `.hex` dans ce dossier `palettes/` (ou dans le dossier utilisateur `%APPDATA%/BentoPack/palettes/`).
- Elles seront scannées et ajoutées automatiquement à la liste déroulante des palettes dans le **Pixel Editor** et le filtre **RetroPalette** au prochain démarrage.
