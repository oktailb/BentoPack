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

#include "extractor/snesromanalyzer.h"
#include "extractor/snessavestateloader.h"
#include <cmath>
#include <algorithm>
#include <QtEndian>

QString SnesDetectedBank::summary() const
{
    QString base = QStringLiteral("0x%1 | %2 | %3 KB (Conf: %4%)")
        .arg(QString::number(offset, 16).toUpper().rightJustified(6, '0'))
        .arg(type)
        .arg(length / 1024)
        .arg(static_cast<int>(confidence * 100));
    return base;
}

// -----------------------------------------------------------------------------
// SnesPaletteScanner
// -----------------------------------------------------------------------------

QList<SnesScoredPalette> SnesPaletteScanner::scanRomPalettes(const QByteArray &romData,
                                                            int maxResults,
                                                            double minScore)
{
    QList<SnesScoredPalette> results;
    const int totalBytes = romData.size();
    if (totalBytes < 32) return results;

    const uchar *raw = reinterpret_cast<const uchar*>(romData.constData());
    
    // Step by 16 or 32 bytes (SNES palettes are 16 words = 32 bytes aligned)
    for (int off = 0; off + 32 <= totalBytes; off += 16) {
        // 1. Bit 15 validity check
        bool allBit15Clear = true;
        quint16 words[16];
        for (int i = 0; i < 16; ++i) {
            words[i] = raw[off + i * 2] | (static_cast<quint16>(raw[off + i * 2 + 1]) << 8);
            if (words[i] & 0x8000) {
                allBit15Clear = false;
                break;
            }
        }
        if (!allBit15Clear) continue;

        // 2. Reject trivial palettes (all black or all white)
        int zeroCount = 0;
        int whiteCount = 0;
        for (int i = 0; i < 16; ++i) {
            if (words[i] == 0x0000) zeroCount++;
            if (words[i] == 0x7FFF) whiteCount++;
        }
        if (zeroCount >= 14 || whiteCount >= 14) continue;

        // 3. Color 0 backdrop bonus: color 0 is transparent/backdrop on SNES
        double score = 0.0;
        if (words[0] == 0x0000) {
            score += 0.25;
        } else {
            // Check if color 0 is very dark
            int r0 = words[0] & 0x1F;
            int g0 = (words[0] >> 5) & 0x1F;
            int b0 = (words[0] >> 10) & 0x1F;
            if (r0 + g0 + b0 < 10) score += 0.15;
        }

        // 4. Calculate perceived luminance: L = 0.299*R + 0.587*G + 0.114*B (normalized 0..100)
        double lum[16];
        double sumLum = 0.0;
        for (int i = 0; i < 16; ++i) {
            int r = ((words[i] & 0x1F) * 255) / 31;
            int g = (((words[i] >> 5) & 0x1F) * 255) / 31;
            int b = (((words[i] >> 10) & 0x1F) * 255) / 31;
            lum[i] = 0.299 * r + 0.587 * g + 0.114 * b;
            sumLum += lum[i];
        }

        // Standard deviation of luminance
        double meanLum = sumLum / 16.0;
        double varLum = 0.0;
        for (int i = 0; i < 16; ++i) {
            double d = lum[i] - meanLum;
            varLum += d * d;
        }
        double stdLum = std::sqrt(varLum / 16.0);

        // Good hand-crafted retro palettes have moderate standard deviation (spread across shades)
        if (stdLum >= 20.0 && stdLum <= 85.0) {
            score += 0.35;
        } else if (stdLum >= 12.0) {
            score += 0.15;
        }

        // 5. Monotonic ramp coherence check: count ascending/descending runs
        int rampSteps = 0;
        for (int i = 2; i < 16; ++i) {
            double diffPrev = lum[i] - lum[i - 1];
            double diffBefore = lum[i - 1] - lum[i - 2];
            if ((diffPrev > 0 && diffBefore > 0) || (diffPrev < 0 && diffBefore < 0)) {
                rampSteps++;
            }
        }
        if (rampSteps >= 5) {
            score += 0.30;
        } else if (rampSteps >= 3) {
            score += 0.15;
        }

        // 6. Distinct colors count
        int distinctColors = 0;
        for (int i = 0; i < 16; ++i) {
            bool unique = true;
            for (int j = 0; j < i; ++j) {
                if (words[i] == words[j]) {
                    unique = false;
                    break;
                }
            }
            if (unique) distinctColors++;
        }
        if (distinctColors >= 8) {
            score += 0.15;
        }

        score = std::clamp(score, 0.0, 1.0);
        if (score >= minScore) {
            SnesScoredPalette sp;
            sp.offset = off;
            sp.score = score;
            sp.colors.resize(16);
            for (int i = 0; i < 16; ++i) {
                sp.colors[i] = SnesSavestateData::bgr555ToRgb(words[i], i == 0);
            }

            // Tag palette based on dominant hue
            double totalR = 0, totalG = 0, totalB = 0;
            for (int i = 1; i < 16; ++i) {
                totalR += qRed(sp.colors[i]);
                totalG += qGreen(sp.colors[i]);
                totalB += qBlue(sp.colors[i]);
            }
            if (totalR > totalG * 1.3 && totalR > totalB * 1.3) {
                sp.tag = QStringLiteral("Warm / Fire / Flesh");
            } else if (totalB > totalR * 1.2 && totalB > totalG * 1.2) {
                sp.tag = QStringLiteral("Cool / Ice / Blue Armor");
            } else if (totalG > totalR * 1.2 && totalG > totalB * 1.2) {
                sp.tag = QStringLiteral("Foliage / Nature Green");
            } else if (std::abs(totalR - totalG) < 200 && std::abs(totalG - totalB) < 200) {
                sp.tag = QStringLiteral("Metallic / Monochrome Gray");
            } else {
                sp.tag = QStringLiteral("Multi-Color Sprite Ramp");
            }

            results.append(sp);
        }
    }

    // Sort descending by score
    std::sort(results.begin(), results.end(), [](const SnesScoredPalette &a, const SnesScoredPalette &b) {
        return a.score > b.score;
    });

    if (results.size() > maxResults) {
        results = results.mid(0, maxResults);
    }
    return results;
}

QStringList SnesPaletteScanner::harmonicPresetNames()
{
    return {
        QStringLiteral("Default Preview Shading"),
        QStringLiteral("Classic Hero (Skin, Green Tunic, Brown)"),
        QStringLiteral("Fiery Warrior (Crimson, Gold, Dark Charcoal)"),
        QStringLiteral("Ice & Steel (Cyan, Deep Cobalt, White)"),
        QStringLiteral("Forest & Earth (Leaf Green, Ochre, Bark)"),
        QStringLiteral("Shadow & Violet (Deep Plum, Lilac, Silver)"),
        QStringLiteral("Metallic Mecha (Graphite, Chrome, Amber Light)"),
        QStringLiteral("Solar Neon (Vibrant Magenta, Electric Gold, Cyan)")
    };
}

QVector<QRgb> SnesPaletteScanner::getHarmonicPresetPalette(int presetId)
{
    QVector<QRgb> pal(16);
    pal[0] = qRgba(0, 0, 0, 0); // Always transparent #0

    switch (presetId) {
    case 1: // Classic Hero (Link / Mario)
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(32, 24, 16),    qRgb(72, 40, 24),   qRgb(128, 72, 40),  // Deep brown / boots
            qRgb(200, 130, 80),  qRgb(240, 184, 136), qRgb(255, 224, 192), // Skin tones
            qRgb(24, 56, 16),    qRgb(48, 104, 32),  qRgb(80, 168, 48),  qRgb(128, 216, 80), // Tunic green
            qRgb(176, 144, 40),  qRgb(240, 208, 72), qRgb(255, 248, 160), // Buckle / Gold
            qRgb(240, 240, 240), qRgb(16, 16, 16)
        };
        break;
    case 2: // Fiery Warrior
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(24, 16, 16),    qRgb(64, 20, 20),   qRgb(112, 28, 28),
            qRgb(168, 36, 36),   qRgb(220, 56, 40),  qRgb(248, 112, 48),
            qRgb(255, 176, 56),  qRgb(255, 232, 120),
            qRgb(48, 40, 48),    qRgb(88, 72, 88),   qRgb(136, 112, 136),
            qRgb(240, 190, 140), qRgb(255, 225, 190),
            qRgb(255, 255, 255), qRgb(16, 16, 16)
        };
        break;
    case 3: // Ice & Steel
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(16, 24, 40),    qRgb(28, 48, 80),   qRgb(48, 80, 128),
            qRgb(72, 120, 184),  qRgb(112, 168, 232), qRgb(168, 216, 255),
            qRgb(224, 244, 255), qRgb(255, 255, 255),
            qRgb(40, 48, 56),    qRgb(72, 84, 96),   qRgb(112, 128, 144),
            qRgb(160, 176, 192), qRgb(216, 228, 240),
            qRgb(248, 216, 120), qRgb(16, 20, 28)
        };
        break;
    case 4: // Forest & Earth
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(20, 28, 16),    qRgb(36, 52, 24),   qRgb(56, 84, 36),
            qRgb(84, 124, 52),   qRgb(124, 172, 72), qRgb(172, 216, 104),
            qRgb(40, 28, 16),    qRgb(72, 48, 28),   qRgb(112, 76, 44),
            qRgb(156, 112, 68),  qRgb(204, 156, 100), qRgb(240, 204, 152),
            qRgb(248, 232, 180), qRgb(255, 255, 255), qRgb(16, 20, 16)
        };
        break;
    case 5: // Shadow & Violet
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(24, 16, 32),    qRgb(48, 28, 64),   qRgb(80, 44, 104),
            qRgb(120, 68, 152),  qRgb(164, 104, 200), qRgb(208, 152, 240),
            qRgb(240, 208, 255), qRgb(48, 48, 56),  qRgb(88, 88, 96),
            qRgb(136, 136, 144), qRgb(192, 192, 200), qRgb(240, 240, 248),
            qRgb(255, 128, 160), qRgb(255, 216, 120), qRgb(16, 12, 20)
        };
        break;
    case 6: // Metallic Mecha
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(24, 24, 28),    qRgb(44, 48, 56),   qRgb(68, 76, 88),
            qRgb(96, 108, 124),  qRgb(132, 144, 164), qRgb(172, 184, 204),
            qRgb(216, 224, 236), qRgb(248, 252, 255),
            qRgb(120, 32, 32),   qRgb(200, 48, 48),  qRgb(255, 80, 80),  // Red sensor
            qRgb(184, 144, 32),  qRgb(248, 208, 48), qRgb(255, 244, 140), // Warning stripes
            qRgb(12, 12, 16)
        };
        break;
    case 7: // Solar Neon
        pal = {
            qRgba(0, 0, 0, 0),
            qRgb(16, 16, 28),    qRgb(48, 20, 56),   qRgb(104, 28, 96),
            qRgb(176, 36, 136),  qRgb(240, 56, 176), qRgb(255, 128, 216),
            qRgb(20, 96, 136),   qRgb(36, 160, 200), qRgb(68, 224, 248),
            qRgb(184, 160, 24),  qRgb(248, 216, 40), qRgb(255, 248, 128),
            qRgb(255, 255, 255), qRgb(255, 80, 80), qRgb(12, 12, 20)
        };
        break;
    default: // 0: Default Preview Shading
        pal = RetroTileDecoder::defaultPreviewPalette(RetroFormat::Snes4bpp);
        break;
    }
    return pal;
}

// -----------------------------------------------------------------------------
// SnesRomAnalyzer
// -----------------------------------------------------------------------------

double SnesRomAnalyzer::calculateShannonEntropy(const uchar *data, int len)
{
    if (!data || len <= 0) return 0.0;

    int freq[256] = {0};
    for (int i = 0; i < len; ++i) {
        freq[data[i]]++;
    }

    double entropy = 0.0;
    double invLen = 1.0 / len;
    for (int i = 0; i < 256; ++i) {
        if (freq[i] > 0) {
            double p = freq[i] * invLen;
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}

double SnesRomAnalyzer::evaluate4bppConfidence(const uchar *data, int len)
{
    // SNES 4bpp tile = 32 bytes (16 bytes planes 0/1, 16 bytes planes 2/3)
    if (!data || len < 32) return 0.0;

    int numTiles = len / 32;
    int validTiles = 0;
    int emptyTiles = 0;

    for (int t = 0; t < numTiles; ++t) {
        const uchar *tile = data + t * 32;

        int p01Count = 0;
        int p23Count = 0;
        int zeroLines01 = 0;
        int zeroLines23 = 0;

        for (int y = 0; y < 8; ++y) {
            uchar b0 = tile[y * 2];
            uchar b1 = tile[y * 2 + 1];
            uchar b2 = tile[16 + y * 2];
            uchar b3 = tile[16 + y * 2 + 1];

            if (b0 == 0 && b1 == 0) zeroLines01++;
            if (b2 == 0 && b3 == 0) zeroLines23++;

            p01Count += (b0 != 0) + (b1 != 0);
            p23Count += (b2 != 0) + (b3 != 0);
        }

        if (zeroLines01 == 8 && zeroLines23 == 8) {
            emptyTiles++;
            continue;
        }

        // Reject uniform / constant byte fill tiles (e.g. repeated 0x55, 0xAA, 0xFF filler)
        bool allIdentical = true;
        for (int b = 1; b < 32; ++b) {
            if (tile[b] != tile[0]) {
                allIdentical = false;
                break;
            }
        }
        if (allIdentical) {
            continue;
        }

        // In hand-crafted SNES pixel art:
        // 1) Base planes 0/1 are usually denser than highlight/shadow planes 2/3
        // 2) Transparent lines match between planes 0/1 and 2/3
        if (p01Count >= p23Count) {
            validTiles++;
        }
    }

    int activeTiles = numTiles - emptyTiles;
    if (activeTiles <= 2) return 0.0;

    double ratio = static_cast<double>(validTiles) / activeTiles;
    // Penalize if 95%+ of tiles are pure empty padding
    if (emptyTiles > numTiles * 0.95) {
        ratio *= 0.5;
    }
    return std::clamp(ratio, 0.0, 1.0);
}

bool SnesRomAnalyzer::decompressLzss(const uchar *src, int srcLen, QByteArray &outData, int maxOut)
{
    if (!src || srcLen < 4) return false;

    // Standard LZSS header: 16-bit uncompressed size
    quint16 decompSize = src[0] | (static_cast<quint16>(src[1]) << 8);
    if (decompSize < 64 || decompSize > maxOut) return false;

    outData.clear();
    outData.reserve(decompSize);

    int inIdx = 2;
    uchar ringBuf[4096];
    std::fill(ringBuf, ringBuf + 4096, 0);
    int ringPos = 4078; // Standard LZSS initial wrap position (4096 - 18)

    quint8 flags = 0;
    int flagBits = 0;

    while (inIdx < srcLen && outData.size() < decompSize) {
        if (flagBits == 0) {
            flags = src[inIdx++];
            flagBits = 8;
        }

        if (flags & 1) {
            // Literal byte
            if (inIdx >= srcLen) break;
            uchar b = src[inIdx++];
            outData.append(static_cast<char>(b));
            ringBuf[ringPos] = b;
            ringPos = (ringPos + 1) & 0x0FFF;
        } else {
            // Match: 12-bit offset, 4-bit length (+3)
            if (inIdx + 1 >= srcLen) break;
            quint16 w = src[inIdx] | (static_cast<quint16>(src[inIdx + 1]) << 8);
            inIdx += 2;

            int matchOffset = w & 0x0FFF;
            int matchLen = ((w >> 12) & 0x0F) + 3;

            for (int k = 0; k < matchLen; ++k) {
                if (outData.size() >= decompSize) break;
                uchar b = ringBuf[(matchOffset + k) & 0x0FFF];
                outData.append(static_cast<char>(b));
                ringBuf[ringPos] = b;
                ringPos = (ringPos + 1) & 0x0FFF;
            }
        }
        flags >>= 1;
        flagBits--;
    }

    return (outData.size() == decompSize);
}

bool SnesRomAnalyzer::decompressRle(const uchar *src, int srcLen, QByteArray &outData, int expectedSize, int maxOut)
{
    if (!src || srcLen < 1) return false;

    int inIdx = 0;
    int targetSize = maxOut;

    if (expectedSize > 0) {
        targetSize = std::min(expectedSize, maxOut);
    } else {
        if (srcLen < 3) return false;
        quint16 decompSize = src[0] | (static_cast<quint16>(src[1]) << 8);
        if (decompSize == 0 || decompSize > maxOut) return false;
        targetSize = decompSize;
        inIdx = 2;
    }

    outData.clear();
    outData.reserve(std::min(targetSize, 65536));

    while (inIdx < srcLen && outData.size() < targetSize) {
        uchar ctrl = src[inIdx++];
        if (ctrl & 0x80) {
            // Repeated byte run
            int count = (ctrl & 0x7F) + 1;
            if (inIdx >= srcLen) break;
            uchar val = src[inIdx++];
            for (int k = 0; k < count && outData.size() < targetSize; ++k) {
                outData.append(static_cast<char>(val));
            }
        } else {
            // Literal run
            int count = (ctrl & 0x7F) + 1;
            for (int k = 0; k < count && inIdx < srcLen && outData.size() < targetSize; ++k) {
                outData.append(static_cast<char>(src[inIdx++]));
            }
        }
    }

    return !outData.isEmpty();
}

bool SnesRomAnalyzer::decompressLcLz2(const uchar *src, int srcLen, QByteArray &outData, int maxOut)
{
    if (!src || srcLen < 4) return false;

    // Nintendo LC-LZ2: 0xFF terminates stream
    outData.clear();
    outData.reserve(4096);

    int inIdx = 0;
    while (inIdx < srcLen && outData.size() < maxOut) {
        uchar b = src[inIdx++];
        if (b == 0xFF) break; // Stream terminator

        int cmd = (b >> 5) & 0x07;
        int len = (b & 0x1F);

        if (len == 0x1F) {
            if (inIdx >= srcLen) break;
            len = src[inIdx++];
        }
        len += 1;

        switch (cmd) {
        case 0: // Direct copy
            for (int k = 0; k < len && inIdx < srcLen && outData.size() < maxOut; ++k) {
                outData.append(static_cast<char>(src[inIdx++]));
            }
            break;
        case 1: { // Byte RLE
            if (inIdx >= srcLen) break;
            uchar val = src[inIdx++];
            for (int k = 0; k < len && outData.size() < maxOut; ++k) {
                outData.append(static_cast<char>(val));
            }
            break;
        }
        case 2: { // Word RLE
            if (inIdx + 1 >= srcLen) break;
            uchar b0 = src[inIdx++];
            uchar b1 = src[inIdx++];
            for (int k = 0; k < len && outData.size() < maxOut; ++k) {
                outData.append(static_cast<char>((k % 2 == 0) ? b0 : b1));
            }
            break;
        }
        case 3: { // Increment
            if (inIdx >= srcLen) break;
            uchar val = src[inIdx++];
            for (int k = 0; k < len && outData.size() < maxOut; ++k) {
                outData.append(static_cast<char>(val + k));
            }
            break;
        }
        case 4: { // LZ back-reference
            if (inIdx + 1 >= srcLen) break;
            quint16 relOffset = src[inIdx] | (static_cast<quint16>(src[inIdx + 1]) << 8);
            inIdx += 2;
            if (relOffset >= outData.size()) break;
            for (int k = 0; k < len && outData.size() < maxOut; ++k) {
                outData.append(outData.at(relOffset + k));
            }
            break;
        }
        default:
            break;
        }
    }

    return (outData.size() >= 128);
}

QList<SnesDetectedBank> SnesRomAnalyzer::scanRomForBanks(const QByteArray &romData, int maxBanks)
{
    QList<SnesDetectedBank> banks;
    const int totalBytes = romData.size();
    if (totalBytes < 512) return banks;

    const uchar *raw = reinterpret_cast<const uchar*>(romData.constData());

    // 1. Scan for uncompressed 4bpp sprite banks (sliding windows of 2KB / 4KB)
    const int blockSize = 2048;
    int currentBankStart = -1;
    int currentBankLen = 0;
    double currentBankConfSum = 0.0;
    int blockCount = 0;

    for (int off = 0; off + blockSize <= totalBytes; off += blockSize) {
        double entropy = calculateShannonEntropy(raw + off, blockSize);
        // Shannon entropy of uncompressed 4bpp sprite data is typically between 3.5 and 6.8
        if (entropy >= 3.2 && entropy <= 6.9) {
            double conf = evaluate4bppConfidence(raw + off, blockSize);
            if (conf >= 0.65) {
                if (currentBankStart < 0) {
                    currentBankStart = off;
                    currentBankLen = blockSize;
                    currentBankConfSum = conf;
                    blockCount = 1;
                } else {
                    currentBankLen += blockSize;
                    currentBankConfSum += conf;
                    blockCount++;
                }
                continue;
            }
        }

        // End of bank run
        if (currentBankStart >= 0) {
            if (currentBankLen >= 2048 && blockCount > 0) {
                SnesDetectedBank b;
                b.offset = currentBankStart;
                b.length = currentBankLen;
                b.confidence = currentBankConfSum / blockCount;
                b.isCompressed = false;
                b.type = QStringLiteral("Uncompressed 4bpp Graphic Bank");
                b.pattern = TilePattern::Sprite16x16_2x2;
                banks.append(b);
            }
            currentBankStart = -1;
            currentBankLen = 0;
            currentBankConfSum = 0.0;
            blockCount = 0;
        }
    }

    if (currentBankStart >= 0 && currentBankLen >= 2048) {
        SnesDetectedBank b;
        b.offset = currentBankStart;
        b.length = currentBankLen;
        b.confidence = currentBankConfSum / blockCount;
        b.isCompressed = false;
        b.type = QStringLiteral("Uncompressed 4bpp Graphic Bank");
        b.pattern = TilePattern::Sprite16x16_2x2;
        banks.append(b);
    }

    // 2. Scan for compressed streams (LZSS / LC-LZ2 / RLE) in high-entropy regions
    // Sample candidate offsets every 256 bytes in areas with entropy > 7.0
    for (int off = 0; off + 64 <= totalBytes && banks.size() < maxBanks; off += 256) {
        double ent = calculateShannonEntropy(raw + off, std::min(512, totalBytes - off));
        if (ent < 6.8) continue; // Skip plain data

        // Trial A: LZSS
        QByteArray lzData;
        if (decompressLzss(raw + off, std::min(8192, totalBytes - off), lzData)) {
            double conf = evaluate4bppConfidence(reinterpret_cast<const uchar*>(lzData.constData()), lzData.size());
            if (conf >= 0.70) {
                SnesDetectedBank b;
                b.offset = off;
                b.length = lzData.size();
                b.confidence = conf;
                b.isCompressed = true;
                b.type = QStringLiteral("Decompressed LZSS Stream");
                b.pattern = TilePattern::Sprite16x16_2x2;
                b.decompressedData = lzData;
                banks.append(b);
                off += 512; // Advance past stream header
                continue;
            }
        }

        // Trial B: LC-LZ2 (Nintendo)
        QByteArray lz2Data;
        if (decompressLcLz2(raw + off, std::min(8192, totalBytes - off), lz2Data)) {
            double conf = evaluate4bppConfidence(reinterpret_cast<const uchar*>(lz2Data.constData()), lz2Data.size());
            if (conf >= 0.70) {
                SnesDetectedBank b;
                b.offset = off;
                b.length = lz2Data.size();
                b.confidence = conf;
                b.isCompressed = true;
                b.type = QStringLiteral("Decompressed Nintendo LC-LZ2");
                b.pattern = TilePattern::Sprite16x16_2x2;
                b.decompressedData = lz2Data;
                banks.append(b);
                off += 512;
                continue;
            }
        }
    }

    // Sort banks descending by confidence
    std::sort(banks.begin(), banks.end(), [](const SnesDetectedBank &a, const SnesDetectedBank &b) {
        return a.confidence > b.confidence;
    });

    if (banks.size() > maxBanks) {
        banks = banks.mid(0, maxBanks);
    }

    return banks;
}
