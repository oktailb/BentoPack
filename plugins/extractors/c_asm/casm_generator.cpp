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

#include "casm_generator.h"
#include <QTextStream>
#include <QDateTime>
#include <QSet>
#include <cmath>

namespace Retro {

QString CAsmGenerator::sanitizeIdentifier(const QString &name)
{
    QString res;
    res.reserve(name.size());
    for (int i = 0; i < name.size(); ++i) {
        QChar c = name.at(i);
        if (c.isLetterOrNumber() || c == QLatin1Char('_')) {
            res.append(c);
        } else {
            res.append(QLatin1Char('_'));
        }
    }
    if (!res.isEmpty() && res.at(0).isDigit()) {
        res.prepend(QLatin1Char('_'));
    }
    return res.isEmpty() ? QStringLiteral("sprite") : res;
}

QString CAsmGenerator::formatByte(uint8_t byte, NumberFormat fmt)
{
    switch (fmt) {
    case Num_Hex_0x:
        return QStringLiteral("0x") + QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    case Num_Hex_Dollar:
        return QStringLiteral("$%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    case Num_Hex_Hash:
        return QStringLiteral("#%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    case Num_Dec:
        return QString::number(byte);
    case Num_Binary: {
        QString bin = QStringLiteral("%");
        for (int b = 7; b >= 0; --b) {
            bin.append((byte & (1 << b)) ? QLatin1Char('1') : QLatin1Char('0'));
        }
        return bin;
    }
    }
    return QStringLiteral("0x") + QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
}

QString CAsmGenerator::formatWord(uint16_t word, NumberFormat fmt)
{
    switch (fmt) {
    case Num_Hex_0x:
        return QStringLiteral("0x") + QStringLiteral("%1").arg(word, 4, 16, QLatin1Char('0')).toUpper();
    case Num_Hex_Dollar:
        return QStringLiteral("$%1").arg(word, 4, 16, QLatin1Char('0')).toUpper();
    case Num_Hex_Hash:
        return QStringLiteral("#%1").arg(word, 4, 16, QLatin1Char('0')).toUpper();
    case Num_Dec:
        return QString::number(word);
    case Num_Binary: {
        QString bin = QStringLiteral("%");
        for (int b = 15; b >= 0; --b) {
            bin.append((word & (1 << b)) ? QLatin1Char('1') : QLatin1Char('0'));
        }
        return bin;
    }
    }
    return QStringLiteral("0x") + QStringLiteral("%1").arg(word, 4, 16, QLatin1Char('0')).toUpper();
}

QString CAsmGenerator::formatByteArray(const QByteArray &data, const CAsmConfig &config, const QString &indent)
{
    if (data.isEmpty()) return QString();

    QString out;
    QTextStream ts(&out);

    bool isAsm = (config.language != Lang_C_Header && config.language != Lang_C_Source && config.language != Lang_PICO8_Lua);
    QString directive;
    if (isAsm) {
        switch (config.language) {
        case Lang_ASM_Motorola_VASM:
            directive = QStringLiteral("dc.b ");
            break;
        case Lang_ASM_RGBDS:
            directive = QStringLiteral("db ");
            break;
        case Lang_ASM_ca65:
            directive = QStringLiteral(".byte ");
            break;
        case Lang_ASM_NASM:
            directive = QStringLiteral("db ");
            break;
        case Lang_ASM_GNU:
            directive = QStringLiteral(".byte ");
            break;
        case Lang_ASM_WLA_DX:
            directive = QStringLiteral(".db ");
            break;
        default:
            directive = QStringLiteral("db ");
            break;
        }
    }

    int bytesPerLine = qBound(4, config.bytesPerLine, 64);

    for (int i = 0; i < data.size(); ++i) {
        if (i % bytesPerLine == 0) {
            if (i > 0) ts << "\n";
            ts << indent;
            if (isAsm) ts << directive;
        }

        uint8_t b = static_cast<uint8_t>(data.at(i));
        ts << formatByte(b, config.numberFormat);

        if (i + 1 < data.size()) {
            if (isAsm) {
                if ((i + 1) % bytesPerLine != 0) {
                    ts << ", ";
                }
            } else {
                ts << ", ";
            }
        }
    }

    return out;
}

uint16_t CAsmGenerator::colorToRGB444(const QColor &c)
{
    // Mega Drive: 4 bits per component, VDP format: 0x0BGR or 0x0RGB
    int r = (c.red() * 15 + 127) / 255;
    int g = (c.green() * 15 + 127) / 255;
    int b = (c.blue() * 15 + 127) / 255;
    return static_cast<uint16_t>((b << 8) | (g << 4) | r);
}

uint16_t CAsmGenerator::colorToBGR555(const QColor &c)
{
    // SNES / GBA: 5 bits per component (0-31), BGR format: bit 0-4 R, bit 5-9 G, bit 10-14 B
    int r = (c.red() * 31 + 127) / 255;
    int g = (c.green() * 31 + 127) / 255;
    int b = (c.blue() * 31 + 127) / 255;
    return static_cast<uint16_t>((b << 10) | (g << 5) | r);
}

QList<QColor> CAsmGenerator::extractDocumentPalette(const SpriteDocument &doc, int maxColors)
{
    QList<QColor> palette;
    palette.append(QColor(0, 0, 0, 0)); // Index 0: Transparent/Background

    QSet<QRgb> seen;
    seen.insert(0);

    const auto &frames = doc.frames();
    for (const QImage &img : frames) {
        if (img.isNull()) continue;
        QImage argb = img.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < argb.height(); ++y) {
            const QRgb *scan = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
            for (int x = 0; x < argb.width(); ++x) {
                QRgb val = scan[x];
                if (qAlpha(val) < 16) continue;
                QRgb opaqueVal = qRgb(qRed(val), qGreen(val), qBlue(val));
                if (!seen.contains(opaqueVal)) {
                    seen.insert(opaqueVal);
                    palette.append(QColor(opaqueVal));
                    if (palette.size() >= maxColors) {
                        return palette;
                    }
                }
            }
        }
    }

    return palette;
}

static inline int findColorIndex(QRgb pixel, const QList<QColor> &palette)
{
    if (qAlpha(pixel) < 16) return 0;
    QRgb opaque = qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel));
    for (int i = 1; i < palette.size(); ++i) {
        if (palette.at(i).rgb() == opaque) return i;
    }
    // Nearest fallback
    int bestIdx = 0;
    int bestDist = 9999999;
    int pr = qRed(pixel);
    int pg = qGreen(pixel);
    int pb = qBlue(pixel);
    for (int i = 0; i < palette.size(); ++i) {
        const QColor &c = palette.at(i);
        int dr = pr - c.red();
        int dg = pg - c.green();
        int db = pb - c.blue();
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

QByteArray CAsmGenerator::encodeLinear8bpp(const QImage &image)
{
    if (image.isNull()) return QByteArray();
    QImage conv = image.convertToFormat(QImage::Format_Indexed8);
    QByteArray bytes;
    bytes.resize(conv.width() * conv.height());
    char *dst = bytes.data();

    for (int y = 0; y < conv.height(); ++y) {
        const uchar *line = conv.constScanLine(y);
        memcpy(dst + y * conv.width(), line, conv.width());
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeLinear4bpp(const QImage &image, bool highFirst)
{
    if (image.isNull()) return QByteArray();
    QImage conv = image.convertToFormat(QImage::Format_ARGB32);
    int w = conv.width();
    int h = conv.height();
    int rowBytes = (w + 1) / 2;
    QByteArray bytes;
    bytes.resize(rowBytes * h);
    bytes.fill(0);

    for (int y = 0; y < h; ++y) {
        const QRgb *scan = reinterpret_cast<const QRgb*>(conv.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb val = scan[x];
            uint8_t index = (qAlpha(val) < 16) ? 0 : static_cast<uint8_t>((qGray(val) >> 4) & 0x0F);
            int bytePos = y * rowBytes + (x / 2);
            uint8_t cur = static_cast<uint8_t>(bytes.at(bytePos));
            if (x % 2 == 0) {
                cur = highFirst ? static_cast<uint8_t>((cur & 0x0F) | (index << 4))
                                : static_cast<uint8_t>((cur & 0xF0) | (index & 0x0F));
            } else {
                cur = highFirst ? static_cast<uint8_t>((cur & 0xF0) | (index & 0x0F))
                                : static_cast<uint8_t>((cur & 0x0F) | (index << 4));
            }
            bytes[bytePos] = static_cast<char>(cur);
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeLinear1bpp(const QImage &image)
{
    if (image.isNull()) return QByteArray();
    QImage conv = image.convertToFormat(QImage::Format_Mono, Qt::MonoOnly);
    int w = conv.width();
    int h = conv.height();
    int rowBytes = (w + 7) / 8;
    QByteArray bytes;
    bytes.resize(rowBytes * h);
    bytes.fill(0);

    for (int y = 0; y < h; ++y) {
        const uchar *line = conv.constScanLine(y);
        memcpy(bytes.data() + y * rowBytes, line, rowBytes);
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeGameBoyTiles(const QImage &image)
{
    if (image.isNull()) return QByteArray();
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int tilesX = (argb.width() + 7) / 8;
    int tilesY = (argb.height() + 7) / 8;

    QByteArray bytes;
    bytes.resize(tilesX * tilesY * 16);
    bytes.fill(0);

    int byteIdx = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            for (int py = 0; py < 8; ++py) {
                int srcY = ty * 8 + py;
                uint8_t plane0 = 0;
                uint8_t plane1 = 0;

                for (int px = 0; px < 8; ++px) {
                    int srcX = tx * 8 + px;
                    int colorIdx = 0;
                    if (srcX < argb.width() && srcY < argb.height()) {
                        QRgb pixel = argb.pixel(srcX, srcY);
                        if (qAlpha(pixel) >= 16) {
                            // Map 0..255 gray to 0..3 shade
                            colorIdx = (3 - (qGray(pixel) >> 6)) & 0x03;
                        }
                    }
                    if (colorIdx & 0x01) plane0 |= (1 << (7 - px));
                    if (colorIdx & 0x02) plane1 |= (1 << (7 - px));
                }

                bytes[byteIdx++] = static_cast<char>(plane0);
                bytes[byteIdx++] = static_cast<char>(plane1);
            }
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeNESTiles(const QImage &image)
{
    if (image.isNull()) return QByteArray();
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int tilesX = (argb.width() + 7) / 8;
    int tilesY = (argb.height() + 7) / 8;

    QByteArray bytes;
    bytes.resize(tilesX * tilesY * 16);
    bytes.fill(0);

    int byteIdx = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            uint8_t plane0[8] = {0};
            uint8_t plane1[8] = {0};

            for (int py = 0; py < 8; ++py) {
                int srcY = ty * 8 + py;
                for (int px = 0; px < 8; ++px) {
                    int srcX = tx * 8 + px;
                    int colorIdx = 0;
                    if (srcX < argb.width() && srcY < argb.height()) {
                        QRgb pixel = argb.pixel(srcX, srcY);
                        if (qAlpha(pixel) >= 16) {
                            colorIdx = (qGray(pixel) >> 6) & 0x03;
                        }
                    }
                    if (colorIdx & 0x01) plane0[py] |= (1 << (7 - px));
                    if (colorIdx & 0x02) plane1[py] |= (1 << (7 - px));
                }
            }

            for (int i = 0; i < 8; ++i) bytes[byteIdx++] = static_cast<char>(plane0[i]);
            for (int i = 0; i < 8; ++i) bytes[byteIdx++] = static_cast<char>(plane1[i]);
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeSega4bppTiles(const QImage &image)
{
    if (image.isNull()) return QByteArray();
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int tilesX = (argb.width() + 7) / 8;
    int tilesY = (argb.height() + 7) / 8;

    QByteArray bytes;
    bytes.resize(tilesX * tilesY * 32);
    bytes.fill(0);

    int byteIdx = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            for (int py = 0; py < 8; ++py) {
                int srcY = ty * 8 + py;
                for (int px = 0; px < 8; px += 2) {
                    uint8_t c0 = 0;
                    uint8_t c1 = 0;

                    int x0 = tx * 8 + px;
                    int x1 = tx * 8 + px + 1;

                    if (x0 < argb.width() && srcY < argb.height()) {
                        QRgb p = argb.pixel(x0, srcY);
                        if (qAlpha(p) >= 16) c0 = (qGray(p) >> 4) & 0x0F;
                    }
                    if (x1 < argb.width() && srcY < argb.height()) {
                        QRgb p = argb.pixel(x1, srcY);
                        if (qAlpha(p) >= 16) c1 = (qGray(p) >> 4) & 0x0F;
                    }

                    bytes[byteIdx++] = static_cast<char>((c0 << 4) | (c1 & 0x0F));
                }
            }
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeAmigaBitplanes(const QImage &image, int numPlanes, bool interleaved)
{
    if (image.isNull()) return QByteArray();
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int w = argb.width();
    int h = argb.height();
    numPlanes = qBound(1, numPlanes, 5);

    int wordsPerRow = (w + 15) / 16;
    int bytesPerRowPerPlane = wordsPerRow * 2;
    int totalBytes = bytesPerRowPerPlane * h * numPlanes;

    QByteArray bytes(totalBytes, 0);

    for (int y = 0; y < h; ++y) {
        for (int p = 0; p < numPlanes; ++p) {
            int planeOffset = interleaved
                ? (y * numPlanes + p) * bytesPerRowPerPlane
                : p * (bytesPerRowPerPlane * h) + (y * bytesPerRowPerPlane);

            for (int x = 0; x < w; ++x) {
                QRgb pixel = argb.pixel(x, y);
                int colorIdx = (qAlpha(pixel) < 16) ? 0 : ((qGray(pixel) >> (8 - numPlanes)) & ((1 << numPlanes) - 1));
                if (colorIdx & (1 << p)) {
                    int byteIdx = planeOffset + (x / 8);
                    int bitIdx = 7 - (x % 8);
                    bytes[byteIdx] = static_cast<char>(bytes.at(byteIdx) | (1 << bitIdx));
                }
            }
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeC64MulticolorSprite(const QImage &image)
{
    // C64 hardware sprite: 24x21 pixels, 63 bytes + 1 padding byte = 64 bytes
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    QByteArray bytes(64, 0);

    int w = qMin(argb.width(), 24);
    int h = qMin(argb.height(), 21);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += 2) {
            int byteIndex = y * 3 + (x / 8);
            int bitShift = 6 - (x % 8);

            QRgb p = argb.pixel(x, y);
            uint8_t c = (qAlpha(p) < 16) ? 0 : ((qGray(p) >> 6) & 0x03);

            uint8_t cur = static_cast<uint8_t>(bytes.at(byteIndex));
            cur |= (c << bitShift);
            bytes[byteIndex] = static_cast<char>(cur);
        }
    }
    return bytes;
}

QByteArray CAsmGenerator::encodeZXSpectrum(const QImage &image)
{
    // 1bpp bitmap + 8x8 attribute cell grid
    if (image.isNull()) return QByteArray();
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int w = argb.width();
    int h = argb.height();

    int rowBytes = (w + 7) / 8;
    QByteArray bitmap(rowBytes * h, 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb p = argb.pixel(x, y);
            if (qAlpha(p) >= 16 && qGray(p) > 127) {
                int bytePos = y * rowBytes + (x / 8);
                bitmap[bytePos] = static_cast<char>(bitmap.at(bytePos) | (1 << (7 - (x % 8))));
            }
        }
    }

    int cellsX = (w + 7) / 8;
    int cellsY = (h + 7) / 8;
    QByteArray attrs(cellsX * cellsY, static_cast<char>(0x47)); // Bright 1, Paper 0 (Black), Ink 7 (White)

    return bitmap + attrs;
}

QByteArray CAsmGenerator::encodeFrame(const QImage &img, const CAsmConfig &config)
{
    switch (config.layout) {
    case Layout_Planar2bppTiles_8x8:
        if (config.targetSystem == System_NES_cc65) {
            return encodeNESTiles(img);
        }
        return encodeGameBoyTiles(img);

    case Layout_Planar4bppTiles_8x8:
        return encodeSega4bppTiles(img);

    case Layout_Linear4bpp_Packed:
        return encodeLinear4bpp(img, config.highNibbleFirst);

    case Layout_Linear1bpp_Mono:
        return encodeLinear1bpp(img);

    case Layout_Amiga_Bitplanes:
        return encodeAmigaBitplanes(img, config.amigaBitplaneCount, config.amigaInterleaved);

    case Layout_C64_Multicolor_Sprite:
        return encodeC64MulticolorSprite(img);

    case Layout_ZX_Spectrum:
        return encodeZXSpectrum(img);

    case Layout_Linear8bpp:
    default:
        return encodeLinear8bpp(img);
    }
}

int CAsmGenerator::calculateRomFootprint(const SpriteDocument &doc, const CAsmConfig &config, int *outTileCount)
{
    int totalBytes = 0;
    int totalTiles = 0;

    const auto &frames = doc.frames();
    for (const QImage &img : frames) {
        if (img.isNull()) continue;
        QByteArray encoded = encodeFrame(img, config);
        totalBytes += encoded.size();

        int tx = (img.width() + 7) / 8;
        int ty = (img.height() + 7) / 8;
        totalTiles += (tx * ty);
    }

    if (config.exportPalette) {
        QList<QColor> pal = extractDocumentPalette(doc, 16);
        totalBytes += pal.size() * 2; // Typically 2 bytes per color in 16-bit systems
    }

    if (config.exportMetadataStruct) {
        totalBytes += doc.frameCount() * 8; // width(1), height(1), pivotX(2), pivotY(2), ptr(4)
    }

    if (outTileCount) *outTileCount = totalTiles;
    return totalBytes;
}

QStringList CAsmGenerator::checkScanlineWarnings(const SpriteDocument &doc, const CAsmConfig &config)
{
    QStringList warnings;

    if (!config.includeLinterComments) {
        return warnings;
    }

    if ((config.targetSystem == System_NES_cc65 || config.targetSystem == System_GameBoy_GBDK) &&
        config.layout == Layout_Planar2bppTiles_8x8) {
        int count = 0;
        int maxWidth = 0;
        for (int i = 0; i < doc.frameCount(); ++i) {
            QImage f = doc.frame(i);
            if (f.width() > 64) {
                count++;
                if (f.width() > maxWidth) maxWidth = f.width();
            }
        }
        if (count == 1) {
            warnings.append(QObject::tr("1 frame exceeds 64 px width (%1 px): will trigger PPU scanline flickering on NES / Game Boy (max 8 sprites of 8px).").arg(maxWidth));
        } else if (count > 1) {
            warnings.append(QObject::tr("%1 frames exceed 64 px width (up to %2 px): will trigger PPU scanline flickering on NES / Game Boy (max 8 sprites of 8px).").arg(count).arg(maxWidth));
        }
    }

    if (config.targetSystem == System_Commodore_64 && config.layout == Layout_C64_Multicolor_Sprite) {
        int count = 0;
        for (int i = 0; i < doc.frameCount(); ++i) {
            QImage f = doc.frame(i);
            if (f.width() > 24 || f.height() > 21) {
                count++;
            }
        }
        if (count == 1) {
            warnings.append(QObject::tr("1 frame exceeds VIC-II hardware sprite size (24x21 px)."));
        } else if (count > 1) {
            warnings.append(QObject::tr("%1 frames exceed VIC-II hardware sprite size (24x21 px).").arg(count));
        }
    }

    return warnings;
}

QString CAsmGenerator::generate(const SpriteDocument &doc, const CAsmConfig &config, const QString &baseName)
{
    QString out;
    QTextStream ts(&out);

    QString idBase = sanitizeIdentifier(baseName.isEmpty() ? doc.projectName() : baseName);
    if (!config.identifierPrefix.isEmpty()) {
        idBase = sanitizeIdentifier(config.identifierPrefix) + QLatin1Char('_') + idBase;
    }
    QString guardName = idBase.toUpper() + QStringLiteral("_H");

    int tileCount = 0;
    int romBytes = calculateRomFootprint(doc, config, &tileCount);
    QStringList warnings = checkScanlineWarnings(doc, config);

    bool isC = (config.language == Lang_C_Header || config.language == Lang_C_Source);
    bool isLua = (config.language == Lang_PICO8_Lua);

    // Header comment block
    if (isC) ts << "/**\n";
    else if (isLua) ts << "--[[\n";
    else ts << "; =============================================================================\n";

    ts << (isC ? " * " : (isLua ? "  " : "; ")) << "BentoPack Studio — C/Assembly Retro Asset Export\n";
    ts << (isC ? " * " : (isLua ? "  " : "; ")) << "Generated on: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    ts << (isC ? " * " : (isLua ? "  " : "; ")) << "Target System: " << [&]() {
        switch (config.targetSystem) {
        case System_MegaDrive_SGDK: return "Sega Mega Drive / Genesis (SGDK - 68000)";
        case System_GameBoy_GBDK: return "Nintendo Game Boy / GBC (GBDK / RGBDS - SM83)";
        case System_NES_cc65: return "Nintendo NES / Famicom (cc65 / NESLib - 6502)";
        case System_Amiga_OCS: return "Commodore Amiga OCS/ECS (VASM / NDK - 68000)";
        case System_Atari_ST: return "Atari ST (VASM / GCC - 68000)";
        case System_SNES_pvsneslib: return "Super Nintendo (pvsneslib - 65816)";
        case System_Commodore_64: return "Commodore 64 (cc65 / KickAssembler - 6502)";
        case System_MasterSystem_devkitSMS: return "Sega Master System (devkitSMS - Z80)";
        case System_Amstrad_CPC: return "Amstrad CPC (cpctelera / RASM - Z80)";
        case System_ZX_Spectrum: return "ZX Spectrum (z88dk / SJASMPLUS - Z80)";
        case System_MSDOS_Mode13h: return "MS-DOS PC VGA Mode 13h (Open Watcom - x86)";
        case System_Pico8: return "PICO-8 Fantasy Console (Lua)";
        default: return "Generic Retro System";
        }
    }() << "\n";
    ts << (isC ? " * " : (isLua ? "  " : "; ")) << "Total ROM Footprint: " << romBytes << " bytes ("
       << QString::number(romBytes / 1024.0, 'f', 2) << " KB), " << tileCount << " tiles (8x8)\n";

    if (config.includeLinterComments && !warnings.isEmpty()) {
        ts << (isC ? " * \n * [!] HARDWARE CONSTRAINTS LINTER WARNINGS:\n" : (isLua ? "  \n  [!] HARDWARE CONSTRAINTS LINTER WARNINGS:\n" : "; \n; [!] HARDWARE CONSTRAINTS LINTER WARNINGS:\n"));
        for (const QString &w : warnings) {
            ts << (isC ? " *  - " : (isLua ? "   - " : ";  - ")) << w << "\n";
        }
    }

    if (isC) ts << " */\n\n";
    else if (isLua) ts << "]]\n\n";
    else ts << "; =============================================================================\n\n";

    // Header Guards for C
    if (config.includeHeaderGuards && config.language == Lang_C_Header) {
        ts << "#ifndef " << guardName << "\n";
        ts << "#define " << guardName << "\n\n";
        ts << "#include <stdint.h>\n";
        ts << "#include <stdbool.h>\n\n";
    }

    // Palette Export
    if (config.exportPalette) {
        QList<QColor> palette = extractDocumentPalette(doc, 16);
        if (!palette.isEmpty()) {
            if (isC) {
                ts << "// Hardware Palette (" << palette.size() << " colors)\n";
                if (config.targetSystem == System_MegaDrive_SGDK) {
                    ts << "static const uint16_t " << idBase << "_palette[" << palette.size() << "] = {\n    ";
                    for (int i = 0; i < palette.size(); ++i) {
                        ts << formatWord(colorToRGB444(palette.at(i)), config.numberFormat);
                        if (i + 1 < palette.size()) ts << ", ";
                    }
                    ts << "\n};\n\n";
                } else if (config.targetSystem == System_SNES_pvsneslib) {
                    ts << "static const u16 " << idBase << "_palette[" << palette.size() << "] = {\n    ";
                    for (int i = 0; i < palette.size(); ++i) {
                        ts << formatWord(colorToBGR555(palette.at(i)), config.numberFormat);
                        if (i + 1 < palette.size()) ts << ", ";
                    }
                    ts << "\n};\n\n";
                } else {
                    ts << "static const uint16_t " << idBase << "_palette[" << palette.size() << "] = {\n    ";
                    for (int i = 0; i < palette.size(); ++i) {
                        ts << formatWord(colorToRGB444(palette.at(i)), config.numberFormat);
                        if (i + 1 < palette.size()) ts << ", ";
                    }
                    ts << "\n};\n\n";
                }
            } else if (!isLua) {
                ts << "; Hardware Palette (" << palette.size() << " colors)\n";
                ts << idBase << "_palette:\n";
                QString wordDirective = (config.language == Lang_ASM_Motorola_VASM) ? QStringLiteral("    dc.w ") : QStringLiteral("    .word ");
                for (int i = 0; i < palette.size(); ++i) {
                    if (i % 8 == 0) {
                        if (i > 0) ts << "\n";
                        ts << wordDirective;
                    }
                    ts << formatWord(colorToRGB444(palette.at(i)), config.numberFormat);
                    if (i + 1 < palette.size() && (i + 1) % 8 != 0) ts << ", ";
                }
                ts << "\n\n";
            }
        }
    }

    // Export each frame data
    QStringList frameVarNames;
    for (int i = 0; i < doc.frameCount(); ++i) {
        QImage frameImg = doc.frame(i);
        if (frameImg.isNull()) continue;

        QString frameName = QStringLiteral("%1_frame_%2").arg(idBase).arg(i, 2, 10, QLatin1Char('0'));
        frameVarNames.append(frameName);

        QByteArray encodedData = encodeFrame(frameImg, config);

        if (isC) {
            ts << "// Frame " << i << ": " << frameImg.width() << "x" << frameImg.height() << " px (" << encodedData.size() << " bytes)\n";
            if (config.useStatic) ts << "static ";
            if (config.useConst) ts << "const ";
            ts << config.customType << " ";
            if (!config.storageQualifier.isEmpty()) {
                ts << config.storageQualifier << " ";
            }
            ts << frameName << "[" << encodedData.size() << "] = {\n";
            ts << formatByteArray(encodedData, config, QStringLiteral("    "));
            ts << "\n};\n\n";
        } else if (isLua) {
            ts << "-- Frame " << i << " (" << frameImg.width() << "x" << frameImg.height() << " px)\n";
            ts << frameName << " = \"";
            for (int b = 0; b < encodedData.size(); ++b) {
                uint8_t byteVal = static_cast<uint8_t>(encodedData.at(b));
                ts << QStringLiteral("\\x%1").arg(byteVal, 2, 16, QLatin1Char('0'));
            }
            ts << "\"\n\n";
        } else {
            ts << "; Frame " << i << ": " << frameImg.width() << "x" << frameImg.height() << " px (" << encodedData.size() << " bytes)\n";
            if (!config.storageQualifier.isEmpty()) {
                ts << "    " << config.storageQualifier << "\n";
            }
            ts << frameName << ":\n";
            ts << formatByteArray(encodedData, config, QStringLiteral("    "));
            ts << "\n\n";
        }
    }

    // Metadata Struct
    if (config.exportMetadataStruct) {
        if (isC) {
            ts << "// Frame Metadata Descriptor\n";
            ts << "typedef struct {\n";
            ts << "    uint8_t width;\n";
            ts << "    uint8_t height;\n";
            ts << "    int8_t  pivot_x;\n";
            ts << "    int8_t  pivot_y;\n";
            ts << "    const " << config.customType << "* data;\n";
            ts << "} " << idBase << "_frame_desc_t;\n\n";

            ts << "static const " << idBase << "_frame_desc_t " << idBase << "_frames[" << doc.frameCount() << "] = {\n";
            for (int i = 0; i < doc.frameCount(); ++i) {
                QImage f = doc.frame(i);
                SpriteBox box = (i < doc.boxes().size()) ? doc.box(i) : SpriteBox(QRect(0, 0, f.width(), f.height()));
                QPoint pivot = box.effectivePivot();
                ts << "    { " << f.width() << ", " << f.height() << ", " << pivot.x() << ", " << pivot.y() << ", " << frameVarNames.value(i) << " }";
                if (i + 1 < doc.frameCount()) ts << ",";
                ts << "\n";
            }
            ts << "};\n\n";
        }
    }

    // Animation Tables
    if (config.exportAnimationTable && !doc.animations().isEmpty()) {
        if (isC) {
            ts << "// Animation Frame Sequences\n";
            for (const auto &anim : doc.animations()) {
                QString animId = sanitizeIdentifier(anim.name.toLower());
                ts << "static const " << config.customType << "* const " << idBase << "_anim_" << animId << "[] = {\n    ";
                for (int k = 0; k < anim.frameIndices.size(); ++k) {
                    int fIdx = anim.frameIndices.at(k);
                    ts << frameVarNames.value(fIdx, QStringLiteral("0"));
                    if (k + 1 < anim.frameIndices.size()) ts << ", ";
                }
                ts << "\n};\n";
                ts << "static const uint16_t " << idBase << "_anim_" << animId << "_count = " << anim.frameIndices.size() << ";\n";
                ts << "static const uint8_t  " << idBase << "_anim_" << animId << "_fps = " << anim.fps << ";\n\n";
            }
        } else if (!isLua) {
            ts << "; Animation Frame Sequences\n";
            QString ptrDirective = (config.language == Lang_ASM_Motorola_VASM) ? QStringLiteral("    dc.l ") : QStringLiteral("    .word ");
            for (const auto &anim : doc.animations()) {
                QString animId = sanitizeIdentifier(anim.name.toLower());
                ts << idBase << "_anim_" << animId << ":\n";
                ts << ptrDirective;
                for (int k = 0; k < anim.frameIndices.size(); ++k) {
                    int fIdx = anim.frameIndices.at(k);
                    ts << frameVarNames.value(fIdx);
                    if (k + 1 < anim.frameIndices.size()) ts << ", ";
                }
                ts << "\n";
                ts << idBase << "_anim_" << animId << "_count: equ " << anim.frameIndices.size() << "\n\n";
            }
        }
    }

    if (config.includeHeaderGuards && config.language == Lang_C_Header) {
        ts << "#endif // " << guardName << "\n";
    }

    return out;
}

} // namespace Retro
