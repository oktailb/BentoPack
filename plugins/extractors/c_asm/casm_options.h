/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CASM_OPTIONS_H
#define CASM_OPTIONS_H

#include <QString>
#include <QSettings>

namespace Retro {

enum TargetSystem {
    System_MegaDrive_SGDK = 0,    // Sega Mega Drive / Genesis (68000, 4bpp packed nibbles 8x8 tiles, SGDK C / VASM)
    System_GameBoy_GBDK,          // Nintendo Game Boy / GBC (SM83/Z80, 2bpp planar 8x8 tiles, GBDK / RGBDS)
    System_NES_cc65,              // Nintendo NES / Famicom (6502, 2bpp planar CHR 8x8 tiles, cc65 / ca65 / NESLib)
    System_Amiga_OCS,             // Commodore Amiga 500/1200 (68000, 1-5 bitplanes planar/interleaved, VASM / GCC)
    System_Atari_ST,              // Atari ST (68000, 4 interleaved bitplanes 320x200 16 colors)
    System_SNES_pvsneslib,        // Super Nintendo (65816, 4bpp planar tiles, pvsneslib / WDC)
    System_Commodore_64,          // Commodore 64 (6502, 24x21 multicolor hardware sprites 64-byte blocks, KickAssembler / cc65)
    System_MasterSystem_devkitSMS,// Sega Master System / Game Gear (Z80, 4bpp planar 8x8 tiles, devkitSMS / WLA-DX)
    System_Amstrad_CPC,           // Amstrad CPC (Z80, Mode 0 16-col / Mode 1 4-col, cpctelera / RASM)
    System_ZX_Spectrum,           // ZX Spectrum (Z80, 1bpp bitmap + 8x8 color attribute grid, z88dk / SJASMPLUS)
    System_MSDOS_Mode13h,         // MS-DOS PC (x86, Mode 13h 320x200 8bpp linear 1 byte/pixel, Open Watcom / NASM)
    System_Pico8,                 // Fantasy Console PICO-8 (4bpp packed nibbles, Lua hex string)
    System_Generic_C              // Generic C/C++ Linear Array (8bpp or 4bpp)
};

enum OutputLanguage {
    Lang_C_Header = 0,            // C/C++ Header (.h)
    Lang_C_Source,                // C Source (.c)
    Lang_ASM_Motorola_VASM,       // Assembly 68000 / VASM (dc.b, dc.w, dc.l, hex $1F)
    Lang_ASM_RGBDS,               // Assembly Game Boy RGBDS (db, dw, dl, hex $1F)
    Lang_ASM_ca65,                // Assembly 6502 ca65 / cc65 (.byte, .word, .dword, hex $1F)
    Lang_ASM_NASM,                // Assembly Intel x86 NASM/TASM (db, dw, dd, hex 0x1F)
    Lang_ASM_GNU,                 // Assembly GNU as / Gas (.byte, .short, .long, hex 0x1F)
    Lang_ASM_WLA_DX,              // Assembly Z80 WLA-DX (.db, .dw, hex $1F)
    Lang_PICO8_Lua                // PICO-8 Lua Table / String
};

enum PixelLayout {
    Layout_Linear8bpp = 0,        // 1 byte per pixel (Indexed 0-255)
    Layout_Linear4bpp_Packed,     // 2 pixels per byte (Packed nibbles: High-first or Low-first)
    Layout_Planar2bppTiles_8x8,   // 8x8 tiles, 2 bitplanes planar (Game Boy, NES) - 16 bytes/tile
    Layout_Planar4bppTiles_8x8,   // 8x8 tiles, 4 bitplanes planar (Master System, Genesis, SNES) - 32 bytes/tile
    Layout_Amiga_Bitplanes,       // 1 to 5 separate or interleaved bitplanes (Amiga OCS/ECS/AGA)
    Layout_C64_Multicolor_Sprite, // 24x21 pixels, 64-byte blocks (63 data + 1 pad)
    Layout_ZX_Spectrum,           // 1bpp monochrome bitmap + 8x8 attribute cell table
    Layout_Linear1bpp_Mono        // 1 bit per pixel (8 pixels per byte)
};

enum NumberFormat {
    Num_Hex_0x = 0,               // 0x1F
    Num_Hex_Dollar,               // $1F
    Num_Hex_Hash,                 // #1F
    Num_Dec,                      // 31
    Num_Binary                    // %00011111 or 0b00011111
};

struct CAsmConfig {
    TargetSystem   targetSystem = System_MegaDrive_SGDK;
    OutputLanguage language = Lang_C_Header;
    PixelLayout    layout = Layout_Planar4bppTiles_8x8;
    NumberFormat   numberFormat = Num_Hex_0x;

    int            bytesPerLine = 16;
    bool           exportPalette = true;
    bool           exportAnimationTable = true;
    bool           exportMetadataStruct = true;
    bool           includeHeaderGuards = true;
    bool           includeLinterComments = true;
    bool           useConst = true;
    bool           useStatic = true;
    QString        customType = QStringLiteral("uint8_t");
    QString        storageQualifier = QString(); // e.g. "PROGMEM", "__far", "__attribute__((aligned(2)))"
    QString        identifierPrefix = QString(); // Optional prefix for symbol names
    int            amigaBitplaneCount = 4;       // 1 to 5 bitplanes
    bool           amigaInterleaved = false;     // Interleaved vs Separate bitplanes
    bool           highNibbleFirst = true;       // For 4bpp packed nibbles

    void applyPreset(TargetSystem system) {
        targetSystem = system;
        switch (system) {
        case System_MegaDrive_SGDK:
            language = Lang_C_Header;
            layout = Layout_Planar4bppTiles_8x8;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("uint16_t");
            storageQualifier = QStringLiteral("__attribute__((aligned(2)))");
            highNibbleFirst = true;
            break;

        case System_GameBoy_GBDK:
            language = Lang_C_Header;
            layout = Layout_Planar2bppTiles_8x8;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("unsigned char");
            storageQualifier = QStringLiteral("const");
            highNibbleFirst = true;
            break;

        case System_NES_cc65:
            language = Lang_ASM_ca65;
            layout = Layout_Planar2bppTiles_8x8;
            numberFormat = Num_Hex_Dollar;
            bytesPerLine = 16;
            customType = QStringLiteral("unsigned char");
            storageQualifier = QString();
            highNibbleFirst = true;
            break;

        case System_Amiga_OCS:
            language = Lang_ASM_Motorola_VASM;
            layout = Layout_Amiga_Bitplanes;
            numberFormat = Num_Hex_Dollar;
            bytesPerLine = 16;
            amigaBitplaneCount = 4;
            amigaInterleaved = false;
            customType = QStringLiteral("UWORD");
            storageQualifier = QStringLiteral("EVEN");
            break;

        case System_Atari_ST:
            language = Lang_ASM_Motorola_VASM;
            layout = Layout_Amiga_Bitplanes;
            amigaBitplaneCount = 4;
            amigaInterleaved = true;
            numberFormat = Num_Hex_Dollar;
            bytesPerLine = 16;
            customType = QStringLiteral("uint16_t");
            storageQualifier = QStringLiteral("EVEN");
            break;

        case System_SNES_pvsneslib:
            language = Lang_C_Header;
            layout = Layout_Planar4bppTiles_8x8;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("u8");
            storageQualifier = QStringLiteral("const");
            highNibbleFirst = true;
            break;

        case System_Commodore_64:
            language = Lang_ASM_ca65;
            layout = Layout_C64_Multicolor_Sprite;
            numberFormat = Num_Hex_Dollar;
            bytesPerLine = 12;
            customType = QStringLiteral("unsigned char");
            storageQualifier = QString();
            break;

        case System_MasterSystem_devkitSMS:
            language = Lang_C_Header;
            layout = Layout_Planar4bppTiles_8x8;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("unsigned char");
            storageQualifier = QStringLiteral("const");
            highNibbleFirst = true;
            break;

        case System_Amstrad_CPC:
            language = Lang_ASM_WLA_DX;
            layout = Layout_Linear4bpp_Packed;
            numberFormat = Num_Hex_Hash;
            bytesPerLine = 16;
            customType = QStringLiteral("uint8_t");
            storageQualifier = QString();
            highNibbleFirst = true;
            break;

        case System_ZX_Spectrum:
            language = Lang_ASM_WLA_DX;
            layout = Layout_ZX_Spectrum;
            numberFormat = Num_Hex_Dollar;
            bytesPerLine = 8;
            customType = QStringLiteral("unsigned char");
            storageQualifier = QString();
            break;

        case System_MSDOS_Mode13h:
            language = Lang_C_Header;
            layout = Layout_Linear8bpp;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("uint8_t");
            storageQualifier = QStringLiteral("__far");
            break;

        case System_Pico8:
            language = Lang_PICO8_Lua;
            layout = Layout_Linear4bpp_Packed;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            highNibbleFirst = false; // PICO-8 memory packs low nibble first
            break;

        case System_Generic_C:
        default:
            language = Lang_C_Header;
            layout = Layout_Linear8bpp;
            numberFormat = Num_Hex_0x;
            bytesPerLine = 16;
            customType = QStringLiteral("uint8_t");
            storageQualifier = QString();
            break;
        }
    }

    void loadFromSettings() {
        QSettings s;
        s.beginGroup(QStringLiteral("Plugins/CAsmExtractor"));
        targetSystem = static_cast<TargetSystem>(s.value(QStringLiteral("targetSystem"), static_cast<int>(System_MegaDrive_SGDK)).toInt());
        language = static_cast<OutputLanguage>(s.value(QStringLiteral("language"), static_cast<int>(language)).toInt());
        layout = static_cast<PixelLayout>(s.value(QStringLiteral("layout"), static_cast<int>(layout)).toInt());
        numberFormat = static_cast<NumberFormat>(s.value(QStringLiteral("numberFormat"), static_cast<int>(numberFormat)).toInt());
        bytesPerLine = s.value(QStringLiteral("bytesPerLine"), bytesPerLine).toInt();
        exportPalette = s.value(QStringLiteral("exportPalette"), exportPalette).toBool();
        exportAnimationTable = s.value(QStringLiteral("exportAnimationTable"), exportAnimationTable).toBool();
        exportMetadataStruct = s.value(QStringLiteral("exportMetadataStruct"), exportMetadataStruct).toBool();
        includeHeaderGuards = s.value(QStringLiteral("includeHeaderGuards"), includeHeaderGuards).toBool();
        includeLinterComments = s.value(QStringLiteral("includeLinterComments"), includeLinterComments).toBool();
        useConst = s.value(QStringLiteral("useConst"), useConst).toBool();
        useStatic = s.value(QStringLiteral("useStatic"), useStatic).toBool();
        customType = s.value(QStringLiteral("customType"), customType).toString();
        storageQualifier = s.value(QStringLiteral("storageQualifier"), storageQualifier).toString();
        identifierPrefix = s.value(QStringLiteral("identifierPrefix"), identifierPrefix).toString();
        amigaBitplaneCount = s.value(QStringLiteral("amigaBitplaneCount"), amigaBitplaneCount).toInt();
        amigaInterleaved = s.value(QStringLiteral("amigaInterleaved"), amigaInterleaved).toBool();
        highNibbleFirst = s.value(QStringLiteral("highNibbleFirst"), highNibbleFirst).toBool();
        s.endGroup();
    }

    void saveToSettings() const {
        QSettings s;
        s.beginGroup(QStringLiteral("Plugins/CAsmExtractor"));
        s.setValue(QStringLiteral("targetSystem"), static_cast<int>(targetSystem));
        s.setValue(QStringLiteral("language"), static_cast<int>(language));
        s.setValue(QStringLiteral("layout"), static_cast<int>(layout));
        s.setValue(QStringLiteral("numberFormat"), static_cast<int>(numberFormat));
        s.setValue(QStringLiteral("bytesPerLine"), bytesPerLine);
        s.setValue(QStringLiteral("exportPalette"), exportPalette);
        s.setValue(QStringLiteral("exportAnimationTable"), exportAnimationTable);
        s.setValue(QStringLiteral("exportMetadataStruct"), exportMetadataStruct);
        s.setValue(QStringLiteral("includeHeaderGuards"), includeHeaderGuards);
        s.setValue(QStringLiteral("includeLinterComments"), includeLinterComments);
        s.setValue(QStringLiteral("useConst"), useConst);
        s.setValue(QStringLiteral("useStatic"), useStatic);
        s.setValue(QStringLiteral("customType"), customType);
        s.setValue(QStringLiteral("storageQualifier"), storageQualifier);
        s.setValue(QStringLiteral("identifierPrefix"), identifierPrefix);
        s.setValue(QStringLiteral("amigaBitplaneCount"), amigaBitplaneCount);
        s.setValue(QStringLiteral("amigaInterleaved"), amigaInterleaved);
        s.setValue(QStringLiteral("highNibbleFirst"), highNibbleFirst);
        s.endGroup();
    }
};

} // namespace Retro

#endif // CASM_OPTIONS_H
