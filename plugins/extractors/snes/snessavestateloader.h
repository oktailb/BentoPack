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

#ifndef SNESSAVESTATELOADER_H
#define SNESSAVESTATELOADER_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QColor>
#include <QtEndian>

/**
 * @brief Container for extracted SNES VRAM (64KB) and CGRAM (512 bytes / 256 BGR555 colors).
 */
struct SnesSavestateData
{
    bool       isValid = false;
    QString    emulatorName;
    QString    romTitle;
    QByteArray vram;   // 65,536 bytes of decompressed PPU tiles
    QByteArray cgram;  // 512 bytes = 256 colors in 15-bit BGR555

    /**
     * @brief Extracts 16 RGB colors for a specific palette (0-7 for BG, 8-15 for Sprites/OBJ).
     * @param paletteIndex Palette index from 0 to 15.
     * @param transparentColorZero If true, color 0 in the palette is given alpha = 0.
     */
    QVector<QRgb> getPalette(int paletteIndex, bool transparentColorZero = true) const;

    /**
     * @brief Returns all 16 palettes (each with 16 colors).
     */
    QVector<QVector<QRgb>> getAllPalettes(bool transparentColorZero = true) const;

    /**
     * @brief Converts a 15-bit SNES BGR555 word into a 32-bit QRgb (with 8-bit channel expansion).
     */
    static QRgb bgr555ToRgb(quint16 word, int alpha = 255);
};

/**
 * @brief Helper for SNES address translation (LoROM, HiROM, copier headers).
 */
class SnesAddress
{
public:
    static bool hasCopierHeader(qint64 fileSize)
    {
        return (fileSize > 512 && (fileSize % 1024 == 512));
    }

    static int snesToPcLoRom(int bank, int addr, bool hasHeader = false)
    {
        int pc = ((bank & 0x7F) * 0x8000) + (addr & 0x7FFF);
        return pc + (hasHeader ? 512 : 0);
    }

    static int snesToPcHiRom(int bank, int addr, bool hasHeader = false)
    {
        int pc = ((bank & 0x3F) * 0x10000) + (addr & 0xFFFF);
        return pc + (hasHeader ? 512 : 0);
    }

    static bool detectHiRom(const QByteArray &data, bool hasHeader);
    static QString readInternalRomTitle(const QByteArray &data, bool hasHeader, bool isHiRom);
};

/**
 * @brief Clean-room loader for SNES emulator savestates (Snes9x, ZSNES, Mesen, raw VRAM/CGRAM dumps).
 */
class SnesSavestateLoader
{
public:
    /**
     * @brief Loads and parses a savestate or memory dump from a file path.
     */
    static SnesSavestateData load(const QString &filePath);

    /**
     * @brief Loads and parses a savestate or memory dump from memory.
     */
    static SnesSavestateData loadFromData(const QByteArray &data, const QString &fileNameHint = QString());

    /**
     * @brief Returns true if the file or data signature matches an emulator savestate.
     */
    static bool isSavestate(const QByteArray &data, const QString &fileNameHint = QString());
};

#endif // SNESSAVESTATELOADER_H
