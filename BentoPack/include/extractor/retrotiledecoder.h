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

#ifndef RETROTILEDECODER_H
#define RETROTILEDECODER_H

#include <QImage>
#include <QByteArray>
#include <QVector>
#include <QColor>
#include <QtEndian>
#include <algorithm>

enum class RetroFormat {
    Nes2bpp,          // 8x8, 2 bitplanes planar (8 bytes plane 0 + 8 bytes plane 1) -> 16 bytes/tile
    GameBoy2bpp,      // 8x8, 2 bitplanes interleaved (plane 0/1 alternating line by line) -> 16 bytes/tile
    Snes3bpp,         // 8x8, 3 bitplanes planar (planes 0/1 interleaved 16 bytes + plane 2 8 bytes) -> 24 bytes/tile
    Snes4bpp,         // 8x8, 4 bitplanes (planes 0/1 interleaved, then planes 2/3 interleaved) -> 32 bytes/tile
    Genesis4bpp,      // 8x8, 4bpp chunky nibbles (2 pixels per byte, 4 bytes per row) -> 32 bytes/tile
    MasterSystem4bpp, // 8x8, 4 bitplanes planar (4 bytes per line: p0, p1, p2, p3) -> 32 bytes/tile
    NeoGeo4bpp,       // 16x16, 4 bitplanes composite (128 bytes per 16x16 sprite tile)
    PsxTim,           // Sony PlayStation 1 TIM file container (4bpp, 8bpp, 16bpp direct)
    RawLinear8bpp     // 8x8, 1 byte per pixel indexed
};

enum class TilePattern {
    Linear,                   // Normal 8x8 linear order (Tiles & Backgrounds)
    Sprite16x16_2x2,          // 16x16 Sprites: 2x2 sequential meta-tiles [0, 1; 2, 3]
    Sprite16x16_VramStride16, // 16x16 Sprites: SNES VRAM 2-row stride 16 (top row 0, bottom row 1)
    Sprite16x16_VramStride32, // 16x16 Sprites: VRAM 2-row stride 32
    Sprite32x32_4x4,          // 32x32 Sprites: 4x4 sequential matrix [0..3; 4..7; 8..11; 12..15]
    Sprite32x32_2x2x4,        // 32x32 Sprites: 4x 2x2 quadrants (SNESTilesKitten p32x32A)
    Sprite32x32_VramStride16  // 32x32 Sprites: SNES VRAM 16-row stride (4 rows separated by 16 tiles)
};

class RetroTileDecoder
{
public:
    static int bytesPerTile(RetroFormat format)
    {
        switch (format) {
        case RetroFormat::Nes2bpp:
        case RetroFormat::GameBoy2bpp:
            return 16;
        case RetroFormat::Snes3bpp:
            return 24;
        case RetroFormat::Snes4bpp:
        case RetroFormat::Genesis4bpp:
        case RetroFormat::MasterSystem4bpp:
            return 32;
        case RetroFormat::NeoGeo4bpp:
            return 128;
        case RetroFormat::RawLinear8bpp:
            return 64;
        case RetroFormat::PsxTim:
            return 0; // Stream-based, container defined
        }
        return 16;
    }

    static int tileSize(RetroFormat format)
    {
        return (format == RetroFormat::NeoGeo4bpp) ? 16 : 8;
    }

    static int paletteSize(RetroFormat format)
    {
        switch (format) {
        case RetroFormat::Nes2bpp:
        case RetroFormat::GameBoy2bpp:
            return 4;
        case RetroFormat::Snes3bpp:
            return 8;
        case RetroFormat::Snes4bpp:
        case RetroFormat::Genesis4bpp:
        case RetroFormat::MasterSystem4bpp:
        case RetroFormat::NeoGeo4bpp:
            return 16;
        default:
            return 256;
        }
    }

    // Default harmonic preview palettes (with transparent color 0 and high-contrast shading)
    static QVector<QRgb> defaultPreviewPalette(RetroFormat format)
    {
        QVector<QRgb> pal;
        if (format == RetroFormat::GameBoy2bpp) {
            pal = {
                qRgba(0, 0, 0, 0),         // 0: Transparent
                qRgb(155, 188, 15),        // 1: Light olive
                qRgb(48, 98, 48),          // 2: Medium green
                qRgb(15, 56, 15)           // 3: Dark olive
            };
        } else if (format == RetroFormat::Nes2bpp) {
            pal = {
                qRgba(0, 0, 0, 0),         // 0: Transparent
                qRgb(84, 84, 84),          // 1: Grey
                qRgb(252, 152, 56),        // 2: Orange/Peach
                qRgb(252, 252, 252)        // 3: White
            };
        } else if (format == RetroFormat::Snes3bpp) {
            pal = {
                qRgba(0, 0, 0, 0),         // 0: Transparent
                qRgb(32, 28, 44),          // 1: Dark
                qRgb(84, 84, 84),          // 2: Grey
                qRgb(168, 77, 85),         // 3: Berry
                qRgb(219, 109, 87),        // 4: Terracotta
                qRgb(255, 214, 153),       // 5: Cream
                qRgb(63, 124, 182),        // 6: Slate blue
                qRgb(245, 245, 245)        // 7: White
            };
        } else {
            // 16-color balanced retro shading palette
            pal = {
                qRgba(0, 0, 0, 0),         // 0: Transparent
                qRgb(32, 28, 44),          // 1: Dark charcoal
                qRgb(65, 48, 79),          // 2: Deep plum
                qRgb(109, 66, 94),         // 3: Muted purple
                qRgb(168, 77, 85),         // 4: Warm berry
                qRgb(219, 109, 87),        // 5: Terracotta
                qRgb(245, 163, 107),       // 6: Peach
                qRgb(255, 214, 153),       // 7: Cream
                qRgb(63, 124, 182),        // 8: Slate blue
                qRgb(108, 178, 209),       // 9: Sky blue
                qRgb(178, 220, 239),       // 10: Ice blue
                qRgb(80, 140, 80),         // 11: Moss green
                qRgb(130, 185, 100),       // 12: Soft green
                qRgb(210, 220, 120),       // 13: Pale lime
                qRgb(245, 245, 245),       // 14: White
                qRgb(20, 20, 20)           // 15: Deep black contour
            };
        }
        return pal;
    }

    static QVector<QRgb> grayscalePalette(int count)
    {
        QVector<QRgb> pal;
        pal.reserve(count);
        pal.append(qRgba(0, 0, 0, 0)); // 0: Transparent
        for (int i = 1; i < count; ++i) {
            int v = (i * 255) / (count - 1);
            pal.append(qRgb(v, v, v));
        }
        return pal;
    }

    // Decode SNES 15-bit BGR555 word (little-endian)
    static inline QRgb snesBgr555ToRgb(quint16 word, bool isTransparent = false)
    {
        if (isTransparent) return qRgba(0, 0, 0, 0);
        int r = ((word & 0x1F) * 255) / 31;
        int g = (((word >> 5) & 0x1F) * 255) / 31;
        int b = (((word >> 10) & 0x1F) * 255) / 31;
        return qRgb(r, g, b);
    }

    // Decode Sega Genesis 9-bit BGR333 word (big-endian 0000_bbb0_ggg0_rrr0)
    static inline QRgb genesisBgr333ToRgb(quint16 word, bool isTransparent = false)
    {
        if (isTransparent) return qRgba(0, 0, 0, 0);
        int r = (((word >> 1) & 0x07) * 255) / 7;
        int g = (((word >> 5) & 0x07) * 255) / 7;
        int b = (((word >> 9) & 0x07) * 255) / 7;
        return qRgb(r, g, b);
    }

    // Read a 16-color palette from raw ROM data at a given offset
    static QVector<QRgb> readRomPalette(const uchar *data, int dataLen, int offset,
                                       RetroFormat format, int colorCount = 16,
                                       bool transparentColor0 = true)
    {
        QVector<QRgb> pal;
        if (offset < 0 || offset + colorCount * 2 > dataLen) {
            return pal;
        }
        pal.reserve(colorCount);

        if (format == RetroFormat::Genesis4bpp) {
            for (int i = 0; i < colorCount; ++i) {
                quint16 w = (static_cast<quint16>(data[offset + i * 2]) << 8) | data[offset + i * 2 + 1];
                pal.append(genesisBgr333ToRgb(w, transparentColor0 && i == 0));
            }
        } else {
            for (int i = 0; i < colorCount; ++i) {
                quint16 w = data[offset + i * 2] | (static_cast<quint16>(data[offset + i * 2 + 1]) << 8);
                pal.append(snesBgr555ToRgb(w, transparentColor0 && i == 0));
            }
        }
        return pal;
    }

    // Decode an 8x8 tile according to specified retro format into a 32-bit pixel buffer
    static void decodeTile8x8(const uchar *data, RetroFormat format, QRgb *dest, int stride,
                              const QVector<QRgb> &pal, bool transparentColor0 = true)
    {
        switch (format) {
        case RetroFormat::Nes2bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = data[y];
                uchar b1 = data[y + 8];
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    int idx = ((b1 >> shift) & 1) << 1 | ((b0 >> shift) & 1);
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::GameBoy2bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = data[y * 2];
                uchar b1 = data[y * 2 + 1];
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    int idx = ((b1 >> shift) & 1) << 1 | ((b0 >> shift) & 1);
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::Snes3bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = data[y * 2];
                uchar b1 = data[y * 2 + 1];
                uchar b2 = data[16 + y];
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    int idx = ((b0 >> shift) & 1)
                            | (((b1 >> shift) & 1) << 1)
                            | (((b2 >> shift) & 1) << 2);
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::Snes4bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = data[y * 2];
                uchar b1 = data[y * 2 + 1];
                uchar b2 = data[16 + y * 2];
                uchar b3 = data[16 + y * 2 + 1];
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    int idx = ((b3 >> shift) & 1) << 3
                            | ((b2 >> shift) & 1) << 2
                            | ((b1 >> shift) & 1) << 1
                            | ((b0 >> shift) & 1);
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::Genesis4bpp: {
            for (int y = 0; y < 8; ++y) {
                const uchar *lineBytes = data + y * 4;
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 4; ++x) {
                    uchar b = lineBytes[x];
                    int idx0 = (b >> 4) & 0x0F;
                    int idx1 = b & 0x0F;
                    row[x * 2]     = (transparentColor0 && idx0 == 0) ? qRgba(0, 0, 0, 0) : pal[idx0 % pal.size()];
                    row[x * 2 + 1] = (transparentColor0 && idx1 == 0) ? qRgba(0, 0, 0, 0) : pal[idx1 % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::MasterSystem4bpp: {
            for (int y = 0; y < 8; ++y) {
                uchar b0 = data[y * 4];
                uchar b1 = data[y * 4 + 1];
                uchar b2 = data[y * 4 + 2];
                uchar b3 = data[y * 4 + 3];
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    int shift = 7 - x;
                    int idx = ((b3 >> shift) & 1) << 3
                            | ((b2 >> shift) & 1) << 2
                            | ((b1 >> shift) & 1) << 1
                            | ((b0 >> shift) & 1);
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        case RetroFormat::RawLinear8bpp: {
            for (int y = 0; y < 8; ++y) {
                QRgb *row = dest + y * stride;
                for (int x = 0; x < 8; ++x) {
                    uchar idx = data[y * 8 + x];
                    row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
                }
            }
            break;
        }
        default:
            break;
        }
    }

    // Decode a Neo-Geo 16x16 sprite tile (128 bytes: 4 quadrants of 8x8 tiles with 4 bitplanes)
    static void decodeTileNeoGeo16x16(const uchar *data, QRgb *dest, int stride,
                                      const QVector<QRgb> &pal, bool transparentColor0 = true)
    {
        for (int y = 0; y < 16; ++y) {
            QRgb *row = dest + y * stride;
            for (int x = 0; x < 16; ++x) {
                int quadX = x / 8;
                int quadY = y / 8;
                int localX = x % 8;
                int localY = y % 8;

                int quadIndex = quadY * 2 + quadX;
                const uchar *quadData = data + quadIndex * 32;

                uchar b0 = quadData[localY * 2];
                uchar b1 = quadData[localY * 2 + 1];
                uchar b2 = quadData[16 + localY * 2];
                uchar b3 = quadData[16 + localY * 2 + 1];

                int shift = 7 - localX;
                int idx = ((b3 >> shift) & 1) << 3
                        | ((b2 >> shift) & 1) << 2
                        | ((b1 >> shift) & 1) << 1
                        | ((b0 >> shift) & 1);
                row[x] = (transparentColor0 && idx == 0) ? qRgba(0, 0, 0, 0) : pal[idx % pal.size()];
            }
        }
    }

    // Decode an entire tile stream into a single QImage spritesheet with pattern layout
    static QImage decodeTileSheet(const uchar *data, int dataLen, RetroFormat format,
                                  int tilesPerRow = 16, int maxTiles = 4096,
                                  const QVector<QRgb> &pal = {}, bool transparentColor0 = true,
                                  TilePattern pattern = TilePattern::Linear)
    {
        int bpt = bytesPerTile(format);
        if (bpt <= 0 || dataLen < bpt) {
            return QImage();
        }

        int tSize = tileSize(format);
        int totalTiles = std::min(dataLen / bpt, maxTiles);
        if (totalTiles <= 0) return QImage();

        int targetWidthTiles = std::clamp(tilesPerRow, 1, 128);

        // Pattern geometry parameters
        int blocksPerRow = 1;
        int numCols = targetWidthTiles;
        int numRows = (totalTiles + numCols - 1) / numCols;

        if (format != RetroFormat::NeoGeo4bpp) {
            switch (pattern) {
            case TilePattern::Linear:
                numCols = targetWidthTiles;
                numRows = (totalTiles + numCols - 1) / numCols;
                break;
            case TilePattern::Sprite16x16_2x2: {
                blocksPerRow = std::max(1, targetWidthTiles / 2);
                numCols = blocksPerRow * 2;
                int totalBlocks = (totalTiles + 3) / 4;
                int blockRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
                numRows = blockRows * 2;
                break;
            }
            case TilePattern::Sprite16x16_VramStride16: {
                blocksPerRow = std::max(1, targetWidthTiles / 16);
                numCols = blocksPerRow * 16;
                int totalBlocks = (totalTiles + 31) / 32;
                int blockRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
                numRows = blockRows * 2;
                break;
            }
            case TilePattern::Sprite16x16_VramStride32: {
                blocksPerRow = std::max(1, targetWidthTiles / 32);
                numCols = blocksPerRow * 32;
                int totalBlocks = (totalTiles + 63) / 64;
                int blockRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
                numRows = blockRows * 2;
                break;
            }
            case TilePattern::Sprite32x32_4x4:
            case TilePattern::Sprite32x32_2x2x4: {
                blocksPerRow = std::max(1, targetWidthTiles / 4);
                numCols = blocksPerRow * 4;
                int totalBlocks = (totalTiles + 15) / 16;
                int blockRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
                numRows = blockRows * 4;
                break;
            }
            case TilePattern::Sprite32x32_VramStride16: {
                blocksPerRow = std::max(1, targetWidthTiles / 16);
                numCols = blocksPerRow * 16;
                int totalBlocks = (totalTiles + 63) / 64;
                int blockRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
                numRows = blockRows * 4;
                break;
            }
            }
        }

        numRows = std::max(1, numRows);
        int imgWidth = numCols * tSize;
        int imgHeight = numRows * tSize;

        QImage sheet(imgWidth, imgHeight, QImage::Format_ARGB32);
        sheet.fill(Qt::transparent);

        QVector<QRgb> activePal = pal;
        if (activePal.isEmpty()) {
            activePal = defaultPreviewPalette(format);
        }

        // SNESTilesKitten p32x32A quadrant mapping
        static const int pat32A[4][4] = {
            { 0,  1,  4,  5},
            { 2,  3,  6,  7},
            { 8,  9, 12, 13},
            {10, 11, 14, 15}
        };

        for (int i = 0; i < totalTiles; ++i) {
            int col = 0;
            int row = 0;

            if (format == RetroFormat::NeoGeo4bpp) {
                col = i % numCols;
                row = i / numCols;
            } else {
                switch (pattern) {
                case TilePattern::Linear:
                    col = i % numCols;
                    row = i / numCols;
                    break;
                case TilePattern::Sprite16x16_2x2: {
                    int b = i / 4;
                    int sub = i % 4;
                    int bx = (b % blocksPerRow) * 2;
                    int by = (b / blocksPerRow) * 2;
                    col = bx + (sub % 2);
                    row = by + (sub / 2);
                    break;
                }
                case TilePattern::Sprite16x16_VramStride16: {
                    int b = i / 32;
                    int sub = i % 32;
                    int bx = (b % blocksPerRow) * 16;
                    int by = (b / blocksPerRow) * 2;
                    if (sub < 16) {
                        col = bx + sub;
                        row = by + 0;
                    } else {
                        col = bx + (sub - 16);
                        row = by + 1;
                    }
                    break;
                }
                case TilePattern::Sprite16x16_VramStride32: {
                    int b = i / 64;
                    int sub = i % 64;
                    int bx = (b % blocksPerRow) * 32;
                    int by = (b / blocksPerRow) * 2;
                    if (sub < 32) {
                        col = bx + sub;
                        row = by + 0;
                    } else {
                        col = bx + (sub - 32);
                        row = by + 1;
                    }
                    break;
                }
                case TilePattern::Sprite32x32_4x4: {
                    int b = i / 16;
                    int sub = i % 16;
                    int bx = (b % blocksPerRow) * 4;
                    int by = (b / blocksPerRow) * 4;
                    col = bx + (sub % 4);
                    row = by + (sub / 4);
                    break;
                }
                case TilePattern::Sprite32x32_2x2x4: {
                    int b = i / 16;
                    int sub = i % 16;
                    int r = 0, c = 0;
                    for (int y = 0; y < 4; ++y) {
                        for (int x = 0; x < 4; ++x) {
                            if (pat32A[y][x] == sub) {
                                r = y; c = x; break;
                            }
                        }
                    }
                    int bx = (b % blocksPerRow) * 4;
                    int by = (b / blocksPerRow) * 4;
                    col = bx + c;
                    row = by + r;
                    break;
                }
                case TilePattern::Sprite32x32_VramStride16: {
                    int b = i / 64;
                    int sub = i % 64;
                    int bx = (b % blocksPerRow) * 16;
                    int by = (b / blocksPerRow) * 4;
                    col = bx + (sub % 16);
                    row = by + (sub / 16);
                    break;
                }
                }
            }

            int px = col * tSize;
            int py = row * tSize;

            if (py + tSize > sheet.height() || px + tSize > sheet.width()) {
                continue;
            }

            const uchar *tileBytes = data + i * bpt;
            QRgb *destTopLeft = reinterpret_cast<QRgb*>(sheet.scanLine(py)) + px;

            if (format == RetroFormat::NeoGeo4bpp) {
                decodeTileNeoGeo16x16(tileBytes, destTopLeft, imgWidth, activePal, transparentColor0);
            } else {
                decodeTile8x8(tileBytes, format, destTopLeft, imgWidth, activePal, transparentColor0);
            }
        }

        return sheet;
    }

    // PSX TIM Decoder
    static bool decodePsxTim(const QByteArray &data, QImage &outImage, QString *errorMessage = nullptr)
    {
        if (data.size() < 16) {
            if (errorMessage) *errorMessage = QStringLiteral("File too small for PSX TIM header.");
            return false;
        }

        const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
        quint32 magic = qFromLittleEndian<quint32>(raw);
        if (magic != 0x00000010) {
            if (errorMessage) *errorMessage = QStringLiteral("Invalid PSX TIM magic signature (expected 0x10).");
            return false;
        }

        quint32 flags = qFromLittleEndian<quint32>(raw + 4);
        int bppMode = flags & 0x07; // 0=4bpp, 1=8bpp, 2=16bpp, 3=24bpp
        bool hasClut = (flags & 0x08) != 0;

        int offset = 8;
        QVector<QRgb> clut;

        if (hasClut) {
            if (offset + 12 > data.size()) {
                if (errorMessage) *errorMessage = QStringLiteral("Truncated PSX TIM CLUT header.");
                return false;
            }
            quint32 clutLen = qFromLittleEndian<quint32>(raw + offset);
            Q_UNUSED(clutLen);
            quint16 clutW = qFromLittleEndian<quint16>(raw + offset + 8);
            quint16 clutH = qFromLittleEndian<quint16>(raw + offset + 10);
            int totalColors = clutW * clutH;

            offset += 12;
            if (offset + totalColors * 2 > data.size()) {
                if (errorMessage) *errorMessage = QStringLiteral("Truncated PSX TIM CLUT entries.");
                return false;
            }

            clut.reserve(totalColors);
            for (int i = 0; i < totalColors; ++i) {
                quint16 c = qFromLittleEndian<quint16>(raw + offset + i * 2);
                int r = (c & 0x1F) << 3;
                int g = ((c >> 5) & 0x1F) << 3;
                int b = ((c >> 10) & 0x1F) << 3;
                int stp = (c >> 15) & 1;
                int a = 255;
                if (c == 0) {
                    a = 0; // Transparent black
                } else if (stp == 1 && (c & 0x7FFF) == 0) {
                    a = 255; // Solid black
                }
                clut.append(qRgba(r, g, b, a));
            }
            offset += totalColors * 2;
        }

        if (offset + 12 > data.size()) {
            if (errorMessage) *errorMessage = QStringLiteral("Truncated PSX TIM pixel data header.");
            return false;
        }

        quint32 imgDataLen = qFromLittleEndian<quint32>(raw + offset);
        Q_UNUSED(imgDataLen);
        quint16 imgWWords = qFromLittleEndian<quint16>(raw + offset + 8);
        quint16 imgH = qFromLittleEndian<quint16>(raw + offset + 10);
        offset += 12;

        int actualWidth = 0;
        if (bppMode == 0)      actualWidth = imgWWords * 4; // 4bpp: 4 pixels per 16-bit word
        else if (bppMode == 1) actualWidth = imgWWords * 2; // 8bpp: 2 pixels per 16-bit word
        else if (bppMode == 2) actualWidth = imgWWords;     // 16bpp: 1 pixel per word
        else if (bppMode == 3) actualWidth = (imgWWords * 2) / 3;

        if (actualWidth <= 0 || imgH <= 0 || actualWidth > 4096 || imgH > 4096) {
            if (errorMessage) *errorMessage = QStringLiteral("Invalid PSX TIM pixel dimensions.");
            return false;
        }

        outImage = QImage(actualWidth, imgH, QImage::Format_ARGB32);
        outImage.fill(Qt::transparent);

        const uchar *pixPtr = raw + offset;
        int bytesRemaining = data.size() - offset;

        for (int y = 0; y < imgH; ++y) {
            QRgb *dstLine = reinterpret_cast<QRgb*>(outImage.scanLine(y));
            if (bppMode == 0) { // 4bpp
                int lineByteLen = actualWidth / 2;
                if ((y + 1) * lineByteLen > bytesRemaining) break;
                const uchar *srcLine = pixPtr + y * lineByteLen;
                for (int x = 0; x < actualWidth; x += 2) {
                    uchar b = srcLine[x / 2];
                    int idx0 = b & 0x0F;        // PSX TIM 4bpp: low nibble first
                    int idx1 = (b >> 4) & 0x0F; // high nibble second
                    dstLine[x]     = (!clut.isEmpty() && idx0 < clut.size()) ? clut[idx0] : qRgb(idx0 * 16, idx0 * 16, idx0 * 16);
                    dstLine[x + 1] = (!clut.isEmpty() && idx1 < clut.size()) ? clut[idx1] : qRgb(idx1 * 16, idx1 * 16, idx1 * 16);
                }
            } else if (bppMode == 1) { // 8bpp
                int lineByteLen = actualWidth;
                if ((y + 1) * lineByteLen > bytesRemaining) break;
                const uchar *srcLine = pixPtr + y * lineByteLen;
                for (int x = 0; x < actualWidth; ++x) {
                    uchar idx = srcLine[x];
                    dstLine[x] = (!clut.isEmpty() && idx < clut.size()) ? clut[idx] : qRgb(idx, idx, idx);
                }
            } else if (bppMode == 2) { // 16bpp Direct BGR555
                int lineByteLen = actualWidth * 2;
                if ((y + 1) * lineByteLen > bytesRemaining) break;
                const uchar *srcLine = pixPtr + y * lineByteLen;
                for (int x = 0; x < actualWidth; ++x) {
                    quint16 c = qFromLittleEndian<quint16>(srcLine + x * 2);
                    if (c == 0) {
                        dstLine[x] = qRgba(0, 0, 0, 0);
                    } else {
                        int r = (c & 0x1F) << 3;
                        int g = ((c >> 5) & 0x1F) << 3;
                        int b = ((c >> 10) & 0x1F) << 3;
                        dstLine[x] = qRgb(r, g, b);
                    }
                }
            }
        }

        return true;
    }
};

#endif // RETROTILEDECODER_H
