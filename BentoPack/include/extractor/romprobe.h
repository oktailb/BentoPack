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

#ifndef ROMPROBE_H
#define ROMPROBE_H

#include "retrotiledecoder.h"
#include <QByteArray>
#include <QString>
#include <QFileInfo>
#include <cstring>
#include <algorithm>

struct GraphicBank {
    int offset = 0;
    int size = 0;
    int tileCount = 0;
    double confidence = 0.0;
    QString label;
};

struct DetectedPalette {
    int offset = 0;
    QVector<QRgb> colors;
    QString label;
    float confidence = 0.0f;
};

struct RomProbeResult {
    RetroFormat format = RetroFormat::Nes2bpp;
    int         offset = 0;
    int         dataLength = 0;
    QString     suggestedPaletteId = QStringLiteral("nes");
    QString     consoleName = QStringLiteral("NES / Famicom");
    bool        detected = false;
    int         tilesPerRow = 16;
    QList<GraphicBank> detectedBanks;
    QList<DetectedPalette> detectedPalettes;
};

class RomProbe
{
public:
    static RomProbeResult probe(const QByteArray &data, const QString &filePath)
    {
        RomProbeResult result;
        result.dataLength = data.size();

        QFileInfo fi(filePath);
        QString ext = fi.suffix().toLower();
        const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
        int size = data.size();

        // 1. PSX TIM Check (Magic 0x10)
        if (size >= 16 && qFromLittleEndian<quint32>(raw) == 0x00000010) {
            result.format = RetroFormat::PsxTim;
            result.offset = 0;
            result.suggestedPaletteId = QStringLiteral("bento_standard");
            result.consoleName = QStringLiteral("Sony PlayStation 1 (TIM)");
            result.detected = true;
            return result;
        }

        // 2. iNES ROM Check ('NES\x1a')
        if (size >= 16 && raw[0] == 'N' && raw[1] == 'E' && raw[2] == 'S' && raw[3] == 0x1A) {
            int prgBanks = raw[4];
            int chrBanks = raw[5];
            int prgSize = prgBanks * 16384;
            int chrSize = chrBanks * 8192;

            result.format = RetroFormat::Nes2bpp;
            result.suggestedPaletteId = QStringLiteral("nes");
            result.consoleName = QStringLiteral("Nintendo NES / Famicom (iNES)");
            result.detected = true;
            result.tilesPerRow = 16;

            if (chrBanks > 0 && 16 + prgSize + chrSize <= size) {
                result.offset = 16 + prgSize;
                result.dataLength = chrSize;
                GraphicBank gb;
                gb.offset = result.offset;
                gb.size = chrSize;
                gb.tileCount = chrSize / 16;
                gb.confidence = 1.0;
                gb.label = QStringLiteral("CHR-ROM 0x%1 (%2 tiles)")
                               .arg(QString::number(gb.offset, 16).toUpper().rightJustified(6, '0'))
                               .arg(gb.tileCount);
                result.detectedBanks.append(gb);
            } else {
                // CHR-RAM game (like Metroid or Mega Man 1): search graphics in PRG
                result.detectedBanks = detectGraphicBanks(data, 16, RetroFormat::Nes2bpp);
                result.offset = result.detectedBanks.isEmpty() ? 16 : result.detectedBanks.first().offset;
                result.dataLength = size - result.offset;
            }
            return result;
        }

        // 3. Game Boy ROM Check (Nintendo Logo at 0x0104 or .gb/.gbc extension)
        bool hasGbLogo = false;
        if (size >= 0x0134) {
            static const uchar gbLogoLead[] = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B };
            hasGbLogo = (std::memcmp(raw + 0x0104, gbLogoLead, 8) == 0);
        }

        if (hasGbLogo || ext == QStringLiteral("gb") || ext == QStringLiteral("gbc")) {
            result.format = RetroFormat::GameBoy2bpp;
            result.suggestedPaletteId = QStringLiteral("gameboy_dmg");
            result.consoleName = QStringLiteral("Nintendo Game Boy / GBC");
            result.detected = true;
            result.tilesPerRow = 16;
            result.detectedBanks = detectGraphicBanks(data, hasGbLogo ? 0x0150 : 0, RetroFormat::GameBoy2bpp);
            result.offset = result.detectedBanks.isEmpty() ? (hasGbLogo ? 0x0150 : 0) : result.detectedBanks.first().offset;
            result.dataLength = size - result.offset;
            return result;
        }

        // 4. Sega Mega Drive / Genesis Check ("SEGA" at 0x0100 or .md/.gen/.smd extension)
        bool hasSegaLogo = false;
        if (size >= 0x0104) {
            hasSegaLogo = (std::memcmp(raw + 0x0100, "SEGA", 4) == 0);
        }

        if (hasSegaLogo || ext == QStringLiteral("md") || ext == QStringLiteral("gen") || ext == QStringLiteral("smd")) {
            result.format = RetroFormat::Genesis4bpp;
            result.suggestedPaletteId = QStringLiteral("sega_megadrive");
            result.consoleName = QStringLiteral("Sega Mega Drive / Genesis");
            result.detected = true;
            result.tilesPerRow = 16;
            result.detectedBanks = detectGraphicBanks(data, hasSegaLogo ? 0x0200 : 0, RetroFormat::Genesis4bpp);
            result.detectedPalettes = scanPalettes(data, RetroFormat::Genesis4bpp);
            result.offset = result.detectedBanks.isEmpty() ? (hasSegaLogo ? 0x0200 : 0) : result.detectedBanks.first().offset;
            result.dataLength = size - result.offset;
            return result;
        }

        // 5. Super Nintendo (.sfc / .smc)
        if (ext == QStringLiteral("sfc") || ext == QStringLiteral("smc")) {
            int headerOffset = (size % 1024 == 512) ? 512 : 0; // SMC copier header
            result.format = RetroFormat::Snes4bpp;
            result.suggestedPaletteId = QStringLiteral("snes");
            result.consoleName = QStringLiteral("Super Nintendo / Super Famicom");
            result.detected = true;
            result.tilesPerRow = 16;
            result.detectedBanks = detectGraphicBanks(data, headerOffset, RetroFormat::Snes4bpp);
            result.detectedPalettes = scanPalettes(data, RetroFormat::Snes4bpp);
            result.offset = result.detectedBanks.isEmpty() ? headerOffset : result.detectedBanks.first().offset;
            result.dataLength = size - result.offset;
            return result;
        }

        // 6. Sega Master System / Game Gear (.sms / .gg)
        if (ext == QStringLiteral("sms") || ext == QStringLiteral("gg")) {
            result.format = RetroFormat::MasterSystem4bpp;
            result.suggestedPaletteId = QStringLiteral("sega_master_system");
            result.consoleName = QStringLiteral("Sega Master System / Game Gear");
            result.detected = true;
            result.tilesPerRow = 16;
            result.detectedBanks = detectGraphicBanks(data, 0, RetroFormat::MasterSystem4bpp);
            result.offset = result.detectedBanks.isEmpty() ? 0 : result.detectedBanks.first().offset;
            result.dataLength = size - result.offset;
            return result;
        }

        // 7. Neo-Geo (.neo, .c1, .c2)
        if (ext == QStringLiteral("neo") || ext == QStringLiteral("c1") || ext == QStringLiteral("c2")) {
            result.format = RetroFormat::NeoGeo4bpp;
            result.suggestedPaletteId = QStringLiteral("neogeo");
            result.consoleName = QStringLiteral("SNK Neo-Geo");
            result.detected = true;
            result.tilesPerRow = 16;
            result.offset = 0;
            result.dataLength = size;
            return result;
        }

        // 8. Dedicated CHR file (.chr) -> NES 2bpp direct
        if (ext == QStringLiteral("chr")) {
            result.format = RetroFormat::Nes2bpp;
            result.suggestedPaletteId = QStringLiteral("nes");
            result.consoleName = QStringLiteral("NES CHR Data");
            result.detected = true;
            result.offset = 0;
            result.dataLength = size;
            result.tilesPerRow = 16;
            return result;
        }

        // 9. VRAM Dumps (.vram, .bin) -> Heuristic format detection
        if (ext == QStringLiteral("vram") || ext == QStringLiteral("bin")) {
            if (size == 8192 || size == 4096) {
                result.format = RetroFormat::Nes2bpp;
                result.suggestedPaletteId = QStringLiteral("nes");
                result.consoleName = QStringLiteral("NES / Game Boy VRAM Dump");
            } else if (size == 65536) {
                result.format = RetroFormat::Genesis4bpp;
                result.suggestedPaletteId = QStringLiteral("sega_megadrive");
                result.consoleName = QStringLiteral("16-Bit VRAM Dump (64KB)");
            } else {
                result.format = RetroFormat::GameBoy2bpp;
                result.suggestedPaletteId = QStringLiteral("gameboy_dmg");
                result.consoleName = QStringLiteral("Raw Tile Dump");
            }
            result.detected = true;
            result.detectedBanks = detectGraphicBanks(data, 0, result.format);
            result.offset = result.detectedBanks.isEmpty() ? 0 : result.detectedBanks.first().offset;
            result.dataLength = size - result.offset;
            result.tilesPerRow = 16;
            return result;
        }

        // Fallback default
        result.format = RetroFormat::Nes2bpp;
        result.offset = 0;
        result.dataLength = size;
        result.suggestedPaletteId = QStringLiteral("nes");
        result.consoleName = QStringLiteral("Generic Tile Stream");
        result.detected = false;
        result.tilesPerRow = 16;
        return result;
    }

    // High-precision graphic bank scanner: analyzes 2D spatial coherence of actual 8x8 decoded tiles
    static QList<GraphicBank> detectGraphicBanks(const QByteArray &data, int startSearch, RetroFormat format, int maxBanks = 24)
    {
        QList<GraphicBank> banks;
        int size = data.size();
        int bpt = RetroTileDecoder::bytesPerTile(format);
        int lineBytes = bpt * 16; // 16 tiles per line

        if (size < startSearch + lineBytes) {
            return banks;
        }

        const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
        int curStart = -1;
        int curEnd = -1;
        double bankScoreSum = 0.0;
        int lineCount = 0;

        for (int pos = startSearch; pos + lineBytes <= size; pos += lineBytes) {
            int validTiles = 0;
            double lineScore = 0.0;
            for (int t = 0; t < 16; ++t) {
                double sc = scoreTileCoherence(raw + pos + t * bpt, format);
                if (sc > 0.0) {
                    validTiles++;
                    lineScore += sc;
                }
            }

            // A line is coherent if at least 6 out of 16 tiles exhibit valid graphic pixel coherence
            bool isCoherentLine = (validTiles >= 6);

            if (isCoherentLine) {
                if (curStart < 0) {
                    curStart = pos;
                }
                curEnd = pos + lineBytes;
                bankScoreSum += (lineScore / 16.0);
                lineCount++;
            } else {
                if (curStart >= 0) {
                    int bankBytes = curEnd - curStart;
                    int tileCount = bankBytes / bpt;
                    // Keep banks of at least 64 tiles (4 lines)
                    if (tileCount >= 64) {
                        GraphicBank gb;
                        gb.offset = curStart;
                        gb.size = bankBytes;
                        gb.tileCount = tileCount;
                        gb.confidence = lineCount > 0 ? (bankScoreSum / lineCount) : 0.0;
                        gb.label = QStringLiteral("0x%1 (%2 tiles, %3% conf)")
                                       .arg(QString::number(curStart, 16).toUpper().rightJustified(6, '0'))
                                       .arg(tileCount)
                                       .arg(static_cast<int>(gb.confidence * 100));
                        banks.append(gb);
                        if (banks.size() >= maxBanks) break;
                    }
                    curStart = -1;
                    curEnd = -1;
                    bankScoreSum = 0.0;
                    lineCount = 0;
                }
            }
        }

        if (curStart >= 0 && banks.size() < maxBanks) {
            int bankBytes = curEnd - curStart;
            int tileCount = bankBytes / bpt;
            if (tileCount >= 64) {
                GraphicBank gb;
                gb.offset = curStart;
                gb.size = bankBytes;
                gb.tileCount = tileCount;
                gb.confidence = lineCount > 0 ? (bankScoreSum / lineCount) : 0.0;
                gb.label = QStringLiteral("0x%1 (%2 tiles, %3% conf)")
                               .arg(QString::number(curStart, 16).toUpper().rightJustified(6, '0'))
                               .arg(tileCount)
                               .arg(static_cast<int>(gb.confidence * 100));
                banks.append(gb);
            }
        }

        return banks;
    }

    static int findGraphicOffset(const QByteArray &data, int startSearch, RetroFormat format)
    {
        QList<GraphicBank> banks = detectGraphicBanks(data, startSearch, format, 4);
        if (!banks.isEmpty()) {
            return banks.first().offset;
        }
        return startSearch;
    }

private:
    static double scoreTileCoherence(const uchar *tileBytes, RetroFormat format)
    {
        uchar pixels[64];
        std::memset(pixels, 0, sizeof(pixels));

        switch (format) {
        case RetroFormat::Snes4bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = tileBytes[y * 2];
                uchar b1 = tileBytes[y * 2 + 1];
                uchar b2 = tileBytes[16 + y * 2];
                uchar b3 = tileBytes[16 + y * 2 + 1];
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    pixels[y * 8 + x] = static_cast<uchar>(
                        ((b0 >> shift) & 1)
                        | (((b1 >> shift) & 1) << 1)
                        | (((b2 >> shift) & 1) << 2)
                        | (((b3 >> shift) & 1) << 3));
                }
            }
            break;
        }
        case RetroFormat::Genesis4bpp: {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 4; ++x) {
                    uchar b = tileBytes[y * 4 + x];
                    pixels[y * 8 + x * 2] = (b >> 4) & 0x0F;
                    pixels[y * 8 + x * 2 + 1] = b & 0x0F;
                }
            }
            break;
        }
        case RetroFormat::GameBoy2bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = tileBytes[y * 2];
                uchar b1 = tileBytes[y * 2 + 1];
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    pixels[y * 8 + x] = static_cast<uchar>(((b0 >> shift) & 1) | (((b1 >> shift) & 1) << 1));
                }
            }
            break;
        }
        case RetroFormat::Nes2bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = tileBytes[y];
                uchar b1 = tileBytes[y + 8];
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    pixels[y * 8 + x] = static_cast<uchar>(((b0 >> shift) & 1) | (((b1 >> shift) & 1) << 1));
                }
            }
            break;
        }
        case RetroFormat::MasterSystem4bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = tileBytes[y * 4];
                uchar b1 = tileBytes[y * 4 + 1];
                uchar b2 = tileBytes[y * 4 + 2];
                uchar b3 = tileBytes[y * 4 + 3];
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    pixels[y * 8 + x] = static_cast<uchar>(
                        ((b0 >> shift) & 1)
                        | (((b1 >> shift) & 1) << 1)
                        | (((b2 >> shift) & 1) << 2)
                        | (((b3 >> shift) & 1) << 3));
                }
            }
            break;
        }
        default:
            return 0.0;
        }

        int colorHistogram[16] = {0};
        int uniqueColors = 0;
        for (int i = 0; i < 64; ++i) {
            uchar c = pixels[i];
            if (c < 16) {
                if (colorHistogram[c] == 0) uniqueColors++;
                colorHistogram[c]++;
            }
        }

        // Blank tile or solid single color
        if (uniqueColors <= 1) return 0.0;
        // High-entropy noise / machine code usually contains too many uniformly scattered colors
        if (uniqueColors > 12) return 0.0;

        int matches = 0;
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 7; ++x) {
                if (pixels[y * 8 + x] == pixels[y * 8 + x + 1]) matches++;
            }
        }
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 8; ++x) {
                if (pixels[y * 8 + x] == pixels[(y + 1) * 8 + x]) matches++;
            }
        }

        double coherence = static_cast<double>(matches) / 112.0; // 56 horiz + 56 vert checks
        if (coherence >= 0.35 && coherence <= 0.88) {
            return coherence;
        }

        return 0.0;
    }

public:
    // Scan ROM data for valid 16-color palettes (SNES/GBA BGR555, Genesis BGR333)
    // 100% generic heuristic analysis: hardware validity, entropy, luminance dynamic range,
    // and artistic ramp continuity. Zero game-specific hardcoding.
    static QList<DetectedPalette> scanPalettes(const QByteArray &data, RetroFormat format, int maxCount = 96)
    {
        QList<DetectedPalette> list;
        const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
        int size = data.size();
        if (size < 32) return list;

        int step = 2; // Check every 16-bit word boundary for SNES / Genesis / GBA

        for (int off = 0; off <= size - 32; off += step) {
            // Hardware validity filter:
            if (format == RetroFormat::Snes4bpp || format == RetroFormat::Snes3bpp) {
                // In SNES BGR555 (0bbbbbgggggrrrrr), bit 15 is unused and must be 0 for all 16 words
                bool valid = true;
                for (int i = 0; i < 16; ++i) {
                    quint16 w = raw[off + i * 2] | (static_cast<quint16>(raw[off + i * 2 + 1]) << 8);
                    if (w & 0x8000) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;
            } else if (format == RetroFormat::Genesis4bpp) {
                // In Genesis BGR333 (0000_bbb0_ggg0_rrr0), high nibble and unused bits are 0
                bool valid = true;
                for (int i = 0; i < 16; ++i) {
                    quint16 w = (static_cast<quint16>(raw[off + i * 2]) << 8) | raw[off + i * 2 + 1];
                    if (w & 0xF111) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;
            }

            QVector<QRgb> pal = RetroTileDecoder::readRomPalette(raw, size, off, format, 16, true);
            if (pal.size() < 16) continue;

            int uniqueCount = 0;
            QVector<QRgb> uniqueCols;
            float maxLum = 0.0f;
            float minLum = 255.0f;
            float lums[16] = {0.0f};

            for (int i = 0; i < 16; ++i) {
                QRgb c = pal[i];
                if (!uniqueCols.contains(c)) {
                    uniqueCols.append(c);
                    uniqueCount++;
                }
                if (i > 0) { // Exclude transparent color 0
                    float lum = 0.299f * qRed(c) + 0.587f * qGreen(c) + 0.114f * qBlue(c);
                    lums[i] = lum;
                    if (lum > maxLum) maxLum = lum;
                    if (lum < minLum) minLum = lum;
                }
            }

            // Real retro palettes have between 6 and 16 unique colors and sufficient contrast
            if (uniqueCount >= 6 && (maxLum - minLum) >= 40.0f) {
                // Evaluate artistic ramp continuity (gradual shading typical of pixel art)
                int rampScore = 0;
                int chromaScore = 0;
                for (int i = 1; i < 15; ++i) {
                    float diff = std::abs(lums[i + 1] - lums[i]);
                    if (diff >= 4.0f && diff <= 65.0f) {
                        rampScore++;
                    }
                    int dr = qRed(pal[i + 1]) - qRed(pal[i]);
                    int dg = qGreen(pal[i + 1]) - qGreen(pal[i]);
                    int db = qBlue(pal[i + 1]) - qBlue(pal[i]);
                    int distSq = dr * dr + dg * dg + db * db;
                    if (distSq >= 100 && distSq <= 14400) { // Color distance 10..120
                        chromaScore++;
                    }
                }

                DetectedPalette dp;
                dp.offset = off;
                dp.colors = pal;
                dp.confidence = static_cast<float>(uniqueCount) * 1.5f + (maxLum - minLum) / 16.0f + rampScore * 2.0f + chromaScore * 1.0f;
                dp.label = QStringLiteral("0x%1 (%2 cols)")
                               .arg(QString::number(off, 16).toUpper().rightJustified(6, '0'))
                               .arg(uniqueCount);
                list.append(dp);
            }
        }

        std::sort(list.begin(), list.end(), [](const DetectedPalette &a, const DetectedPalette &b) {
            return a.confidence > b.confidence;
        });

        // Filter / deduplicate closely adjacent palette hits (keep highest confidence per 32-byte window)
        QList<DetectedPalette> deduped;
        QSet<int> takenOffsets;
        for (const auto &dp : list) {
            bool tooClose = false;
            for (int taken : takenOffsets) {
                if (std::abs(dp.offset - taken) < 32) {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose) {
                deduped.append(dp);
                takenOffsets.insert(dp.offset);
                if (deduped.size() >= maxCount) break;
            }
        }

        return deduped;
    }
};

#endif // ROMPROBE_H
