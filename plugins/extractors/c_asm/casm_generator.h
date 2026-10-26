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

#ifndef CASM_GENERATOR_H
#define CASM_GENERATOR_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QImage>
#include <QColor>
#include "casm_options.h"
#include "model/spritedocument.h"

namespace Retro {

class CAsmGenerator
{
public:
    static QString generate(const SpriteDocument &doc, const CAsmConfig &config, const QString &baseName);

    // Memory footprint calculation
    static int calculateRomFootprint(const SpriteDocument &doc, const CAsmConfig &config, int *outTileCount = nullptr);

    // Hardware linter checks
    static QStringList checkScanlineWarnings(const SpriteDocument &doc, const CAsmConfig &config);

    // Low-level pixel layout encoders
    static QByteArray encodeFrame(const QImage &image, const CAsmConfig &config);
    static QByteArray encodeLinear8bpp(const QImage &image);
    static QByteArray encodeLinear4bpp(const QImage &image, bool highFirst = true);
    static QByteArray encodeLinear1bpp(const QImage &image);
    static QByteArray encodeGameBoyTiles(const QImage &image);
    static QByteArray encodeNESTiles(const QImage &image);
    static QByteArray encodeSega4bppTiles(const QImage &image);
    static QByteArray encodeAmigaBitplanes(const QImage &image, int numPlanes, bool interleaved);
    static QByteArray encodeC64MulticolorSprite(const QImage &image);
    static QByteArray encodeZXSpectrum(const QImage &image);

    // Palette extraction and color conversion
    static QList<QColor> extractDocumentPalette(const SpriteDocument &doc, int maxColors = 256);
    static uint16_t colorToRGB444(const QColor &color); // Mega Drive: 0x0RGB
    static uint16_t colorToBGR555(const QColor &color); // SNES / GBA: 0xBBGGGRRR

    // Formatting helpers
    static QString formatByte(uint8_t byte, NumberFormat fmt);
    static QString formatWord(uint16_t word, NumberFormat fmt);
    static QString formatByteArray(const QByteArray &data, const CAsmConfig &config, const QString &indent = QStringLiteral("    "));
    static QString sanitizeIdentifier(const QString &name);
};

} // namespace Retro

#endif // CASM_GENERATOR_H
