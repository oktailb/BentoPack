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

#include "retrotileextractor.h"
#include "image/spritedetector.h"
#include "image/colorpalettepresets.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

RetroTileExtractor::RetroTileExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool RetroTileExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (supportedExtensions().contains(ext)) {
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    uchar hdr[16];
    qint64 readBytes = file.read(reinterpret_cast<char*>(hdr), 16);
    if (readBytes >= 4) {
        // iNES magic
        if (hdr[0] == 'N' && hdr[1] == 'E' && hdr[2] == 'S' && hdr[3] == 0x1A) {
            return true;
        }
        // PSX TIM magic
        if (qFromLittleEndian<quint32>(hdr) == 0x00000010) {
            return true;
        }
    }

    return false;
}

bool RetroTileExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open retro ROM/VRAM file: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("File is empty: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    RomProbeResult probeResult = RomProbe::probe(data, filePath);

    QImage atlas;
    if (probeResult.format == RetroFormat::PsxTim) {
        QString timErr;
        if (!RetroTileDecoder::decodePsxTim(data, atlas, &timErr)) {
            if (error) {
                error->code = ExtractorError::CorruptedData;
                error->message = tr("Failed to decode PSX TIM: %1").arg(timErr);
                error->filePath = filePath;
            }
            return false;
        }
    } else {
        // Use harmonic retro preview palette (transparent 0, shaded ramps)
        QVector<QRgb> pal = RetroTileDecoder::defaultPreviewPalette(probeResult.format);
        if (pal.isEmpty()) {
            pal = ColorPalettePresets::getPaletteById(probeResult.suggestedPaletteId);
            if (pal.isEmpty()) {
                pal = ColorPalettePresets::standardPalette();
            }
        }

        const uchar *raw = reinterpret_cast<const uchar*>(data.constData());
        int availableBytes = data.size() - probeResult.offset;
        if (availableBytes < RetroTileDecoder::bytesPerTile(probeResult.format)) {
            if (error) {
                error->code = ExtractorError::CorruptedData;
                error->message = tr("Insufficient tile data at offset %1.").arg(probeResult.offset);
                error->filePath = filePath;
            }
            return false;
        }

        const uchar *tileStream = raw + probeResult.offset;
        atlas = RetroTileDecoder::decodeTileSheet(
            tileStream,
            availableBytes,
            probeResult.format,
            probeResult.tilesPerRow,
            4096, // Maximum 4096 tiles in one sheet
            pal,
            true  // Color 0 transparent
        );
    }

    if (atlas.isNull()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Failed to decode retro tile stream.");
            error->filePath = filePath;
        }
        return false;
    }

    // Auto-slicing: identify character sprites and items via SpriteDetector
    QList<QImage> frameImages;
    QList<SpriteBox> boxes;

    SpriteDetectionOptions detOpts;
    detOpts.alphaThreshold = 1;
    detOpts.minSliceSize = 4; // Ignore isolated single-pixel specks

    if (!SpriteDetector::detectToImages(atlas, frameImages, boxes, detOpts) || frameImages.isEmpty()) {
        // Fallback: full atlas as a single frame
        SpriteBox singleBox(QRect(0, 0, atlas.width(), atlas.height()));
        boxes.append(singleBox);
        frameImages.append(atlas);
    }

    outDoc.setAtlas(atlas);
    outDoc.setFrames(frameImages, boxes);
    outDoc.setFilePath(filePath);
    return true;
}

bool RetroTileExtractor::encodeTiles(const QImage &image, RetroFormat format, QByteArray &outData, QString *errorMsg)
{
    if (image.isNull()) {
        if (errorMsg) *errorMsg = QStringLiteral("Cannot encode null image to retro tiles.");
        return false;
    }

    int tSize = RetroTileDecoder::tileSize(format);
    int numTilesX = image.width() / tSize;
    int numTilesY = image.height() / tSize;

    if (numTilesX <= 0 || numTilesY <= 0) {
        if (errorMsg) *errorMsg = QStringLiteral("Image dimensions smaller than a single tile.");
        return false;
    }

    // Quantize image to appropriate bit-depth
    int palCount = RetroTileDecoder::paletteSize(format);
    QImage indexed = image;
    if (indexed.format() != QImage::Format_Indexed8) {
        indexed = image.convertToFormat(QImage::Format_Indexed8, Qt::AutoColor);
    }

    int bpt = RetroTileDecoder::bytesPerTile(format);
    outData.clear();
    outData.reserve(numTilesX * numTilesY * bpt);

    QByteArray tileBuf(bpt, 0);

    for (int ty = 0; ty < numTilesY; ++ty) {
        for (int tx = 0; tx < numTilesX; ++tx) {
            tileBuf.fill(0);
            int originX = tx * tSize;
            int originY = ty * tSize;

            switch (format) {
            case RetroFormat::Nes2bpp: {
                for (int y = 0; y < 8; ++y) {
                    uchar b0 = 0;
                    uchar b1 = 0;
                    const uchar *line = indexed.constScanLine(originY + y) + originX;
                    for (int x = 0; x < 8; ++x) {
                        uchar idx = line[x] % palCount;
                        if (qAlpha(image.pixel(originX + x, originY + y)) < 128) idx = 0;
                        int shift = 7 - x;
                        if (idx & 1) b0 |= (1 << shift);
                        if (idx & 2) b1 |= (1 << shift);
                    }
                    tileBuf.data()[y] = b0;
                    tileBuf.data()[y + 8] = b1;
                }
                break;
            }
            case RetroFormat::GameBoy2bpp: {
                for (int y = 0; y < 8; ++y) {
                    uchar b0 = 0;
                    uchar b1 = 0;
                    const uchar *line = indexed.constScanLine(originY + y) + originX;
                    for (int x = 0; x < 8; ++x) {
                        uchar idx = line[x] % palCount;
                        if (qAlpha(image.pixel(originX + x, originY + y)) < 128) idx = 0;
                        int shift = 7 - x;
                        if (idx & 1) b0 |= (1 << shift);
                        if (idx & 2) b1 |= (1 << shift);
                    }
                    tileBuf.data()[y * 2] = b0;
                    tileBuf.data()[y * 2 + 1] = b1;
                }
                break;
            }
            case RetroFormat::Genesis4bpp: {
                for (int y = 0; y < 8; ++y) {
                    const uchar *line = indexed.constScanLine(originY + y) + originX;
                    for (int i = 0; i < 4; ++i) {
                        uchar idx0 = line[i * 2] % palCount;
                        uchar idx1 = line[i * 2 + 1] % palCount;
                        if (qAlpha(image.pixel(originX + i * 2, originY + y)) < 128) idx0 = 0;
                        if (qAlpha(image.pixel(originX + i * 2 + 1, originY + y)) < 128) idx1 = 0;
                        tileBuf.data()[y * 4 + i] = static_cast<char>((idx0 << 4) | (idx1 & 0x0F));
                    }
                }
                break;
            }
            case RetroFormat::Snes4bpp: {
                for (int y = 0; y < 8; ++y) {
                    uchar b0 = 0, b1 = 0, b2 = 0, b3 = 0;
                    const uchar *line = indexed.constScanLine(originY + y) + originX;
                    for (int x = 0; x < 8; ++x) {
                        uchar idx = line[x] % palCount;
                        if (qAlpha(image.pixel(originX + x, originY + y)) < 128) idx = 0;
                        int shift = 7 - x;
                        if (idx & 1) b0 |= (1 << shift);
                        if (idx & 2) b1 |= (1 << shift);
                        if (idx & 4) b2 |= (1 << shift);
                        if (idx & 8) b3 |= (1 << shift);
                    }
                    tileBuf.data()[y * 2] = b0;
                    tileBuf.data()[y * 2 + 1] = b1;
                    tileBuf.data()[16 + y * 2] = b2;
                    tileBuf.data()[16 + y * 2 + 1] = b3;
                }
                break;
            }
            default:
                break;
            }

            outData.append(tileBuf);
        }
    }

    return true;
}

bool RetroTileExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);

    if (inDoc.atlas().isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    RetroFormat format = RetroFormat::Nes2bpp;
    if (ext == QStringLiteral("gb") || ext == QStringLiteral("gbc")) {
        format = RetroFormat::GameBoy2bpp;
    } else if (ext == QStringLiteral("md") || ext == QStringLiteral("gen") || ext == QStringLiteral("smd")) {
        format = RetroFormat::Genesis4bpp;
    } else if (ext == QStringLiteral("sfc") || ext == QStringLiteral("smc")) {
        format = RetroFormat::Snes4bpp;
    } else {
        format = RetroFormat::Nes2bpp;
    }

    QByteArray tileData;
    QString errStr;
    if (!encodeTiles(inDoc.atlas(), format, tileData, &errStr)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to encode retro tiles: %1").arg(errStr);
            error->filePath = filePath;
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot open file for writing: %1").arg(file.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    if (file.write(tileData) != tileData.size()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to write all tile bytes to disk.");
            error->filePath = filePath;
        }
        return false;
    }

    return true;
}
