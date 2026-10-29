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

#ifndef SNESROMANALYZER_H
#define SNESROMANALYZER_H

#include <QByteArray>
#include <QVector>
#include <QList>
#include <QString>
#include <QColor>
#include "extractor/retrotiledecoder.h"

struct SnesScoredPalette
{
    int             offset = 0;
    double          score = 0.0;
    QVector<QRgb>   colors;
    QString         tag;
};

struct SnesDetectedBank
{
    int             offset = 0;
    int             length = 0;
    double          confidence = 0.0;
    bool            isCompressed = false;
    QString         type; // "Uncompressed 4bpp", "LZSS Stream", "Nintendo LC-LZ2", "RLE Stream"
    TilePattern     pattern = TilePattern::Sprite16x16_2x2;
    QByteArray      decompressedData;

    QString summary() const;
};

class SnesPaletteScanner
{
public:
    /**
     * @brief Scans a raw ROM for contiguous 32-byte blocks matching SNES BGR555 palette characteristics.
     * Evaluates bit 15 validity, color 0 backdrop, luminance ramp monotonicity, and chromatic variance.
     */
    static QList<SnesScoredPalette> scanRomPalettes(const QByteArray &romData,
                                                    int maxResults = 32,
                                                    double minScore = 0.40);

    /**
     * @brief Returns hand-crafted harmonic SNES palettes covering classic archetypes
     * (Hero, Monster, Fire, Ice, Forest, Metal, Earth, Neon).
     */
    static QVector<QRgb> getHarmonicPresetPalette(int presetId);
    static QStringList harmonicPresetNames();
};

class SnesRomAnalyzer
{
public:
    /**
     * @brief Computes Shannon entropy (bits per byte) over a sliding block.
     * Range: 0.0 (uniform) to 8.0 (pure noise / high compression).
     */
    static double calculateShannonEntropy(const uchar *data, int len);

    /**
     * @brief Evaluates correlation between bitplanes 0/1 and 2/3 typical of hand-crafted SNES 4bpp tiles.
     * Returns a confidence metric between 0.0 and 1.0.
     */
    static double evaluate4bppConfidence(const uchar *data, int len);

    /**
     * @brief Scans an entire ROM to identify sprite graphic regions (uncompressed and compressed).
     */
    static QList<SnesDetectedBank> scanRomForBanks(const QByteArray &romData, int maxBanks = 64);

    // Clean-room, sandbox-safe decompressors
    static bool decompressLzss(const uchar *src, int srcLen, QByteArray &outData, int maxOut = 65536);
    static bool decompressRle(const uchar *src, int srcLen, QByteArray &outData, int expectedSize = 0, int maxOut = 65536);
    static bool decompressLcLz2(const uchar *src, int srcLen, QByteArray &outData, int maxOut = 65536);
};

#endif // SNESROMANALYZER_H
