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

#include "asepriteextractor.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QPainter>
#include <QMap>
#include <cstring>

AsepriteExtractor::AsepriteExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool AsepriteExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (ext != QStringLiteral("ase") && ext != QStringLiteral("aseprite")) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    if (file.size() < 128) {
        return false;
    }

    QByteArray header = file.read(128);
    file.close();

    // Check Aseprite header magic at offset 4: 0xA5E0
    const quint8 *ptr = reinterpret_cast<const quint8*>(header.constData());
    quint16 magic = ptr[4] | (ptr[5] << 8);
    return (magic == 0xA5E0);
}

QByteArray AsepriteExtractor::decompressZlib(const char *data, int compressedSize, quint32 expectedUncompressedSize)
{
    if (compressedSize <= 0 || !data) return QByteArray();

    // qUncompress expects 4-byte expected uncompressed size in big-endian before zlib stream
    QByteArray buf;
    buf.resize(4 + compressedSize);
    buf[0] = static_cast<char>((expectedUncompressedSize >> 24) & 0xFF);
    buf[1] = static_cast<char>((expectedUncompressedSize >> 16) & 0xFF);
    buf[2] = static_cast<char>((expectedUncompressedSize >> 8) & 0xFF);
    buf[3] = static_cast<char>(expectedUncompressedSize & 0xFF);
    std::memcpy(buf.data() + 4, data, compressedSize);

    return qUncompress(buf);
}

QImage AsepriteExtractor::decodePixelsToImage(const QByteArray &rawPixels, int width, int height,
                                             quint16 colorDepth, const QVector<QRgb> &palette,
                                             quint8 transparentIndex)
{
    if (width <= 0 || height <= 0 || rawPixels.isEmpty()) {
        return QImage();
    }

    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    const uchar *src = reinterpret_cast<const uchar*>(rawPixels.constData());

    if (colorDepth == 32) {
        // RGBA: 4 bytes per pixel (R, G, B, A)
        int srcLen = rawPixels.size();
        for (int y = 0; y < height; ++y) {
            QRgb *scanline = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int offset = (y * width + x) * 4;
                if (offset + 3 < srcLen) {
                    quint8 r = src[offset];
                    quint8 g = src[offset + 1];
                    quint8 b = src[offset + 2];
                    quint8 a = src[offset + 3];
                    scanline[x] = qRgba(r, g, b, a);
                } else {
                    scanline[x] = 0;
                }
            }
        }
    } else if (colorDepth == 16) {
        // Grayscale: 2 bytes per pixel (Value, Alpha)
        int srcLen = rawPixels.size();
        for (int y = 0; y < height; ++y) {
            QRgb *scanline = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int offset = (y * width + x) * 2;
                if (offset + 1 < srcLen) {
                    quint8 val = src[offset];
                    quint8 a   = src[offset + 1];
                    scanline[x] = qRgba(val, val, val, a);
                } else {
                    scanline[x] = 0;
                }
            }
        }
    } else if (colorDepth == 8) {
        // Indexed: 1 byte per pixel
        int srcLen = rawPixels.size();
        for (int y = 0; y < height; ++y) {
            QRgb *scanline = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < width; ++x) {
                int offset = y * width + x;
                if (offset < srcLen) {
                    quint8 idx = src[offset];
                    if (idx == transparentIndex) {
                        scanline[x] = 0;
                    } else if (idx < palette.size()) {
                        scanline[x] = palette.at(idx);
                    } else {
                        scanline[x] = 0;
                    }
                } else {
                    scanline[x] = 0;
                }
            }
        }
    }

    return img;
}

bool AsepriteExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading Aseprite project binary..."));
    setProgress(10);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open Aseprite file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.size() < 128) {
        if (error) {
            error->code = ExtractorError::InvalidHeader;
            error->message = tr("File is too small to be a valid Aseprite file.");
            error->filePath = filePath;
        }
        return false;
    }

    const quint8 *raw = reinterpret_cast<const quint8*>(fileData.constData());
    int dataSize = fileData.size();

    // 1. Header parsing (128 bytes)
    quint16 magic = raw[4] | (raw[5] << 8);
    if (magic != 0xA5E0) {
        if (error) {
            error->code = ExtractorError::InvalidHeader;
            error->message = tr("Invalid Aseprite magic header (expected 0xA5E0, got 0x%1)").arg(magic, 4, 16, QLatin1Char('0'));
            error->filePath = filePath;
        }
        return false;
    }

    quint16 numFrames  = raw[6] | (raw[7] << 8);
    quint16 canvasWidth = raw[8] | (raw[9] << 8);
    quint16 canvasHeight = raw[10] | (raw[11] << 8);
    quint16 colorDepth = raw[12] | (raw[13] << 8);
    quint8  transparentIndex = raw[28];

    if (canvasWidth == 0 || canvasHeight == 0 || numFrames == 0) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Invalid canvas dimensions or zero frames in Aseprite header.");
            error->filePath = filePath;
        }
        return false;
    }

    // Initialize default palette (256 entries)
    QVector<QRgb> palette(256, qRgba(0, 0, 0, 255));
    palette[transparentIndex] = qRgba(0, 0, 0, 0);

    QList<AseLayer> layers;
    QList<AseTag> animationTags;
    QList<AseSlice> slices;

    struct FrameData {
        int durationMs = 100;
        QList<AseCel> cels;
    };
    QList<FrameData> frames;
    frames.reserve(numFrames);

    // 2. Parse frames
    int offset = 128;

    for (int frameIdx = 0; frameIdx < numFrames && offset < dataSize; ++frameIdx) {
        if (offset + 16 > dataSize) break;

        quint32 frameBytes = raw[offset] | (raw[offset + 1] << 8) | (raw[offset + 2] << 16) | (raw[offset + 3] << 24);
        quint16 frameMagic = raw[offset + 4] | (raw[offset + 5] << 8);
        quint16 oldChunks  = raw[offset + 6] | (raw[offset + 7] << 8);
        quint16 frameDuration = raw[offset + 8] | (raw[offset + 9] << 8);
        quint32 numChunks = oldChunks;

        if (frameMagic != 0xF1FA) {
            break;
        }

        if (numChunks == 0xFFFF) {
            numChunks = raw[offset + 12] | (raw[offset + 13] << 8) | (raw[offset + 14] << 16) | (raw[offset + 15] << 24);
        }

        FrameData currentFrame;
        currentFrame.durationMs = (frameDuration > 0) ? frameDuration : 100;

        int frameEnd = offset + frameBytes;
        if (frameEnd > dataSize) frameEnd = dataSize;

        int chunkOffset = offset + 16;

        for (quint32 c = 0; c < numChunks && chunkOffset < frameEnd; ++c) {
            if (chunkOffset + 6 > frameEnd) break;

            quint32 chunkSize = raw[chunkOffset] | (raw[chunkOffset + 1] << 8) |
                                (raw[chunkOffset + 2] << 16) | (raw[chunkOffset + 3] << 24);
            quint16 chunkType = raw[chunkOffset + 4] | (raw[chunkOffset + 5] << 8);

            if (chunkSize < 6 || static_cast<int>(chunkOffset + chunkSize) > frameEnd) {
                break;
            }

            const quint8 *chunkData = raw + chunkOffset + 6;
            int payloadSize = chunkSize - 6;

            switch (chunkType) {
            case 0x2004: { // Layer chunk
                if (payloadSize >= 18) {
                    AseLayer layer;
                    layer.flags = chunkData[0] | (chunkData[1] << 8);
                    layer.type = chunkData[2] | (chunkData[3] << 8);
                    layer.childLevel = chunkData[4] | (chunkData[5] << 8);
                    layer.blendMode = chunkData[10] | (chunkData[11] << 8);
                    layer.opacity = chunkData[12];

                    quint16 nameLen = chunkData[16] | (chunkData[17] << 8);
                    if (18 + nameLen <= payloadSize) {
                        layer.name = QString::fromUtf8(reinterpret_cast<const char*>(chunkData + 18), nameLen);
                    }
                    layers.append(layer);
                }
                break;
            }
            case 0x2005: { // Cel chunk
                if (payloadSize >= 14) {
                    AseCel cel;
                    cel.layerIndex = chunkData[0] | (chunkData[1] << 8);
                    cel.x = static_cast<qint16>(chunkData[2] | (chunkData[3] << 8));
                    cel.y = static_cast<qint16>(chunkData[4] | (chunkData[5] << 8));
                    cel.opacity = chunkData[6];
                    cel.celType = chunkData[7] | (chunkData[8] << 8);

                    if (cel.celType == 0) { // Raw Cel
                        if (payloadSize >= 20) {
                            cel.width = chunkData[16] | (chunkData[17] << 8);
                            cel.height = chunkData[18] | (chunkData[19] << 8);
                            int bpp = (colorDepth == 32) ? 4 : (colorDepth == 16 ? 2 : 1);
                            int expectedBytes = cel.width * cel.height * bpp;
                            if (20 + expectedBytes <= payloadSize) {
                                QByteArray rawPix(reinterpret_cast<const char*>(chunkData + 20), expectedBytes);
                                cel.image = decodePixelsToImage(rawPix, cel.width, cel.height, colorDepth, palette, transparentIndex);
                            }
                        }
                    } else if (cel.celType == 1) { // Linked Cel
                        if (payloadSize >= 16) {
                            cel.linkedFrame = chunkData[14] | (chunkData[15] << 8);
                        }
                    } else if (cel.celType == 2) { // Compressed Cel (zlib)
                        if (payloadSize >= 20) {
                            cel.width = chunkData[16] | (chunkData[17] << 8);
                            cel.height = chunkData[18] | (chunkData[19] << 8);
                            int bpp = (colorDepth == 32) ? 4 : (colorDepth == 16 ? 2 : 1);
                            quint32 expectedBytes = cel.width * cel.height * bpp;

                            int compSize = payloadSize - 20;
                            QByteArray uncompressed = decompressZlib(reinterpret_cast<const char*>(chunkData + 20), compSize, expectedBytes);
                            if (!uncompressed.isEmpty()) {
                                cel.image = decodePixelsToImage(uncompressed, cel.width, cel.height, colorDepth, palette, transparentIndex);
                            }
                        }
                    }
                    currentFrame.cels.append(cel);
                }
                break;
            }
            case 0x2018: { // Frame Tags chunk (Animations)
                if (payloadSize >= 10) {
                    quint16 numTags = chunkData[0] | (chunkData[1] << 8);
                    int tagOffset = 10;
                    for (quint16 t = 0; t < numTags && tagOffset + 17 <= payloadSize; ++t) {
                        AseTag tag;
                        tag.fromFrame = chunkData[tagOffset] | (chunkData[tagOffset + 1] << 8);
                        tag.toFrame   = chunkData[tagOffset + 2] | (chunkData[tagOffset + 3] << 8);
                        tag.loopDirection = chunkData[tagOffset + 4];
                        tag.repeatCount   = chunkData[tagOffset + 5] | (chunkData[tagOffset + 6] << 8);

                        // Tag name string is at tagOffset + 17
                        if (tagOffset + 19 <= payloadSize) {
                            quint16 nameLen = chunkData[tagOffset + 17] | (chunkData[tagOffset + 18] << 8);
                            if (tagOffset + 19 + nameLen <= payloadSize) {
                                tag.name = QString::fromUtf8(reinterpret_cast<const char*>(chunkData + tagOffset + 19), nameLen);
                            }
                            tagOffset += 19 + nameLen;
                        } else {
                            tagOffset += 17;
                        }
                        animationTags.append(tag);
                    }
                }
                break;
            }
            case 0x2019: { // Palette chunk
                if (payloadSize >= 20) {
                    quint32 newPalSize = chunkData[0] | (chunkData[1] << 8) | (chunkData[2] << 16) | (chunkData[3] << 24);
                    quint32 firstIdx   = chunkData[4] | (chunkData[5] << 8) | (chunkData[6] << 16) | (chunkData[7] << 24);
                    quint32 lastIdx    = chunkData[8] | (chunkData[9] << 8) | (chunkData[10] << 16) | (chunkData[11] << 24);

                    if (newPalSize > static_cast<quint32>(palette.size())) {
                        palette.resize(newPalSize);
                    }

                    int palOffset = 20;
                    for (quint32 p = firstIdx; p <= lastIdx && palOffset + 6 <= payloadSize; ++p) {
                        quint16 flags = chunkData[palOffset] | (chunkData[palOffset + 1] << 8);
                        quint8 r = chunkData[palOffset + 2];
                        quint8 g = chunkData[palOffset + 3];
                        quint8 b = chunkData[palOffset + 4];
                        quint8 a = chunkData[palOffset + 5];

                        if (p < static_cast<quint32>(palette.size())) {
                            palette[p] = (p == transparentIndex) ? qRgba(0, 0, 0, 0) : qRgba(r, g, b, a);
                        }

                        palOffset += 6;
                        if (flags & 1) { // Has name string
                            if (palOffset + 2 <= payloadSize) {
                                quint16 nameLen = chunkData[palOffset] | (chunkData[palOffset + 1] << 8);
                                palOffset += 2 + nameLen;
                            }
                        }
                    }
                }
                break;
            }
            case 0x2022: { // Slices chunk
                if (payloadSize >= 16) {
                    quint32 numKeys = chunkData[0] | (chunkData[1] << 8) | (chunkData[2] << 16) | (chunkData[3] << 24);
                    quint32 flags   = chunkData[4] | (chunkData[5] << 8) | (chunkData[6] << 16) | (chunkData[7] << 24);
                    quint16 nameLen = chunkData[12] | (chunkData[13] << 8);
                    int sliceOffset = 14;
                    QString sliceName;
                    if (sliceOffset + nameLen <= payloadSize) {
                        sliceName = QString::fromUtf8(reinterpret_cast<const char*>(chunkData + sliceOffset), nameLen);
                        sliceOffset += nameLen;
                    }

                    for (quint32 k = 0; k < numKeys && sliceOffset + 20 <= payloadSize; ++k) {
                        qint32 sx = static_cast<qint32>(chunkData[sliceOffset + 4] | (chunkData[sliceOffset + 5] << 8) |
                                                        (chunkData[sliceOffset + 6] << 16) | (chunkData[sliceOffset + 7] << 24));
                        qint32 sy = static_cast<qint32>(chunkData[sliceOffset + 8] | (chunkData[sliceOffset + 9] << 8) |
                                                        (chunkData[sliceOffset + 10] << 16) | (chunkData[sliceOffset + 11] << 24));
                        quint32 sw = chunkData[sliceOffset + 12] | (chunkData[sliceOffset + 13] << 8) |
                                     (chunkData[sliceOffset + 14] << 16) | (chunkData[sliceOffset + 15] << 24);
                        quint32 sh = chunkData[sliceOffset + 16] | (chunkData[sliceOffset + 17] << 8) |
                                     (chunkData[sliceOffset + 18] << 16) | (chunkData[sliceOffset + 19] << 24);
                        sliceOffset += 20;

                        AseSlice slice;
                        slice.name = sliceName;
                        slice.bounds = QRect(sx, sy, sw, sh);

                        if (flags & 1) { // 9-slice
                            sliceOffset += 16;
                        }
                        if (flags & 2) { // Pivot
                            if (sliceOffset + 8 <= payloadSize) {
                                qint32 px = static_cast<qint32>(chunkData[sliceOffset] | (chunkData[sliceOffset + 1] << 8) |
                                                                (chunkData[sliceOffset + 2] << 16) | (chunkData[sliceOffset + 3] << 24));
                                qint32 py = static_cast<qint32>(chunkData[sliceOffset + 4] | (chunkData[sliceOffset + 5] << 8) |
                                                                (chunkData[sliceOffset + 6] << 16) | (chunkData[sliceOffset + 7] << 24));
                                slice.pivot = QPoint(px, py);
                                slice.hasPivot = true;
                                sliceOffset += 8;
                            }
                        }
                        slices.append(slice);
                    }
                }
                break;
            }
            default:
                break;
            }

            chunkOffset += chunkSize;
        }

        frames.append(currentFrame);
        offset = frameEnd;
        setProgress(10 + (frameIdx * 40 / qMax(1, static_cast<int>(numFrames))));
    }

    if (frames.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Failed to decode any frames from Aseprite file.");
            error->filePath = filePath;
        }
        return false;
    }

    // 3. Composite frames into QImages & populate Layers / Cels (M17)
    outDoc.clear();
    outDoc.setFilePath(filePath);

    QList<SpriteLayer> docLayers;
    if (!layers.isEmpty()) {
        docLayers.reserve(layers.size());
        for (int i = 0; i < layers.size(); ++i) {
            const AseLayer &al = layers.at(i);
            SpriteLayer sl;
            sl.id = QStringLiteral("layer_%1").arg(i);
            sl.name = al.name.isEmpty() ? QStringLiteral("Layer %1").arg(i + 1) : al.name;
            sl.visible = al.isVisible();
            sl.locked = (al.flags & 2) == 0;
            sl.opacity = al.opacity;
            sl.zOrder = i;
            sl.childLevel = al.childLevel;
            sl.type = (al.type == 1) ? SpriteLayer::Group : (al.type == 2 ? SpriteLayer::Tilemap : SpriteLayer::Normal);

            switch (al.blendMode) {
            case 1: sl.blendMode = QPainter::CompositionMode_Multiply; break;
            case 2: sl.blendMode = QPainter::CompositionMode_Screen; break;
            case 3: sl.blendMode = QPainter::CompositionMode_Overlay; break;
            case 4: sl.blendMode = QPainter::CompositionMode_Darken; break;
            case 5: sl.blendMode = QPainter::CompositionMode_Lighten; break;
            case 6: sl.blendMode = QPainter::CompositionMode_ColorDodge; break;
            case 7: sl.blendMode = QPainter::CompositionMode_ColorBurn; break;
            case 8: sl.blendMode = QPainter::CompositionMode_HardLight; break;
            case 9: sl.blendMode = QPainter::CompositionMode_SoftLight; break;
            case 10: sl.blendMode = QPainter::CompositionMode_Difference; break;
            case 11: sl.blendMode = QPainter::CompositionMode_Exclusion; break;
            case 16: sl.blendMode = QPainter::CompositionMode_Plus; break;
            default: sl.blendMode = QPainter::CompositionMode_SourceOver; break;
            }

            docLayers.append(sl);
        }
    } else {
        SpriteLayer sl;
        sl.id = QStringLiteral("layer_0");
        sl.name = QStringLiteral("Layer 1");
        sl.visible = true;
        sl.opacity = 255;
        docLayers.append(sl);
    }
    outDoc.setLayers(docLayers);

    int totalCompositeFrames = frames.size();
    QList<QImage> composedImages;
    composedImages.reserve(totalCompositeFrames);

    for (int f = 0; f < totalCompositeFrames; ++f) {
        const FrameData &fd = frames.at(f);
        QList<SpriteCel> docCels;
        docCels.reserve(fd.cels.size());

        QImage frameCanvas(canvasWidth, canvasHeight, QImage::Format_ARGB32_Premultiplied);
        frameCanvas.fill(Qt::transparent);
        QPainter p(&frameCanvas);

        for (const AseCel &cel : fd.cels) {
            QImage celImg = cel.image;
            if (cel.celType == 1 && cel.linkedFrame < frames.size()) {
                // Find cel from linked frame
                for (const AseCel &linkedCel : frames.at(cel.linkedFrame).cels) {
                    if (linkedCel.layerIndex == cel.layerIndex) {
                        celImg = linkedCel.image;
                        break;
                    }
                }
            }

            if (!celImg.isNull()) {
                SpriteCel sc;
                sc.layerIndex = cel.layerIndex;
                if (cel.layerIndex < docLayers.size()) {
                    sc.layerId = docLayers.at(cel.layerIndex).id;
                } else {
                    sc.layerId = QStringLiteral("layer_%1").arg(cel.layerIndex);
                }
                sc.x = cel.x;
                sc.y = cel.y;
                sc.opacity = cel.opacity;
                sc.image = celImg;
                docCels.append(sc);

                // Draw to composite frameCanvas if layer is visible
                bool lyrVis = (cel.layerIndex < layers.size()) ? layers.at(cel.layerIndex).isVisible() : true;
                if (lyrVis) {
                    if (cel.opacity < 255) {
                        p.setOpacity(cel.opacity / 255.0);
                    } else {
                        p.setOpacity(1.0);
                    }
                    p.drawImage(QPoint(cel.x, cel.y), celImg);
                }
            }
        }
        p.end();

        outDoc.setFrameCels(f, docCels);
        composedImages.append(frameCanvas);
        SpriteBox box(QRect(0, 0, canvasWidth, canvasHeight));
        box.index = f;
        outDoc.addFrame(frameCanvas, box);

        setProgress(50 + (f * 30 / qMax(1, totalCompositeFrames)));
    }

    // 3b. Detect candidate skin / variant profiles from layer naming conventions
    QMap<QString, QStringList> variantGroups;
    QStringList baseLayerIds;

    for (const SpriteLayer &sl : docLayers) {
        int sepIdx = sl.name.indexOf(QLatin1Char(':'));
        if (sepIdx < 0) sepIdx = sl.name.indexOf(QLatin1Char('/'));

        if (sepIdx > 0) {
            QString group = sl.name.left(sepIdx).trimmed();
            variantGroups[group].append(sl.id);
        } else {
            baseLayerIds.append(sl.id);
        }
    }

    if (!variantGroups.isEmpty()) {
        for (auto git = variantGroups.constBegin(); git != variantGroups.constEnd(); ++git) {
            const QStringList &varLayerIds = git.value();
            for (const QString &vId : varLayerIds) {
                int lyrIdx = outDoc.findLayerIndexById(vId);
                QString lyrName = (lyrIdx >= 0) ? docLayers.at(lyrIdx).name : vId;
                int sep = lyrName.indexOf(QLatin1Char(':'));
                if (sep < 0) sep = lyrName.indexOf(QLatin1Char('/'));
                QString profName = (sep >= 0) ? lyrName.mid(sep + 1).trimmed() : lyrName;
                SkinProfile prof;
                prof.id = QStringLiteral("skin_%1").arg(vId);
                prof.name = profName;
                prof.activeLayerIds = baseLayerIds;
                prof.activeLayerIds.append(vId);
                outDoc.addSkinProfile(prof);
            }
        }
    }

    // 4. Compose default atlas layout (side-by-side or grid) so outDoc.atlas() is populated
    int cols = qMax(1, static_cast<int>(std::ceil(std::sqrt(totalCompositeFrames))));
    int rows = (totalCompositeFrames + cols - 1) / cols;
    QImage atlasImg(cols * canvasWidth, rows * canvasHeight, QImage::Format_ARGB32_Premultiplied);
    atlasImg.fill(Qt::transparent);

    QPainter atlasPainter(&atlasImg);
    for (int i = 0; i < totalCompositeFrames; ++i) {
        int cx = (i % cols) * canvasWidth;
        int cy = (i / cols) * canvasHeight;
        atlasPainter.drawImage(QPoint(cx, cy), composedImages.at(i));
        outDoc.updateBoxRect(i, QRect(cx, cy, canvasWidth, canvasHeight));
    }
    atlasPainter.end();
    outDoc.setAtlas(atlasImg);

    // 5. Register animation tracks from Frame Tags
    int avgDurationMs = frames.first().durationMs;
    int fps = (avgDurationMs > 0) ? qBound(1, 1000 / avgDurationMs, 60) : 12;

    if (!animationTags.isEmpty()) {
        for (const AseTag &tag : animationTags) {
            if (tag.name.isEmpty()) continue;

            QList<int> animFrames;
            int start = qMin(static_cast<int>(tag.fromFrame), totalCompositeFrames - 1);
            int end   = qMin(static_cast<int>(tag.toFrame), totalCompositeFrames - 1);

            if (start <= end) {
                for (int idx = start; idx <= end; ++idx) {
                    animFrames.append(idx);
                }
            } else {
                for (int idx = start; idx >= end; --idx) {
                    animFrames.append(idx);
                }
            }

            SpriteAnimation::LoopMode mode = SpriteAnimation::Loop;
            if (tag.loopDirection == 1) { // Reverse
                std::reverse(animFrames.begin(), animFrames.end());
                mode = SpriteAnimation::Loop;
            } else if (tag.loopDirection == 2) { // Ping-pong
                mode = SpriteAnimation::PingPong;
            }

            outDoc.addAnimation(tag.name, animFrames, fps, mode);
        }
    } else if (totalCompositeFrames > 1) {
        // Register default animation if multiple frames exist
        QList<int> allIndices;
        for (int i = 0; i < totalCompositeFrames; ++i) allIndices.append(i);
        outDoc.addAnimation(QStringLiteral("default"), allIndices, fps, SpriteAnimation::Loop);
    }

    setProgress(100);
    setStatusMessage(tr("Aseprite project imported successfully (%1 frames).").arg(totalCompositeFrames));
    if (error) error->code = ExtractorError::NoError;
    return true;
}

bool AsepriteExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);
    setStatusMessage(tr("Exporting native Aseprite project binary..."));
    setProgress(10);

    if (inDoc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Document contains no sprite frames to export to Aseprite.");
            error->filePath = filePath;
        }
        return false;
    }

    int numFrames = inDoc.frameCount();
    int canvasW = inDoc.maxFrameWidth();
    int canvasH = inDoc.maxFrameHeight();
    if (canvasW <= 0 || canvasH <= 0) {
        for (int i = 0; i < numFrames; ++i) {
            QImage f = inDoc.frame(i);
            canvasW = qMax(canvasW, f.width());
            canvasH = qMax(canvasH, f.height());
        }
    }
    if (canvasW <= 0) canvasW = 32;
    if (canvasH <= 0) canvasH = 32;

    int fps = 12;
    if (!inDoc.animations().isEmpty()) {
        fps = inDoc.animations().first().fps;
        if (fps <= 0) fps = 12;
    }
    quint16 frameDurationMs = static_cast<quint16>(qBound(1, 1000 / fps, 65535));

    QByteArray fileBuf;
    QDataStream ds(&fileBuf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    // 1. Header (128 bytes)
    ds << static_cast<quint32>(0);          // Total file size placeholder (offset 0)
    ds << static_cast<quint16>(0xA5E0);     // Magic (offset 4)
    ds << static_cast<quint16>(numFrames);  // Frames count (offset 6)
    ds << static_cast<quint16>(canvasW);    // Width (offset 8)
    ds << static_cast<quint16>(canvasH);    // Height (offset 10)
    ds << static_cast<quint16>(32);         // 32bpp RGBA (offset 12)
    ds << static_cast<quint32>(1);          // Flags: layer has opacity (offset 14)
    ds << static_cast<quint16>(frameDurationMs); // Speed (offset 18)
    ds << static_cast<quint32>(0);          // Reserved (offset 20)
    ds << static_cast<quint32>(0);          // Reserved (offset 24)
    ds << static_cast<quint8>(0);           // Transparent index (offset 28)
    ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0); // Ignore (offset 29..31)
    ds << static_cast<quint16>(256);        // Number of colors (offset 32)
    ds << static_cast<quint8>(1) << static_cast<quint8>(1); // Pixel aspect ratio (offset 34..35)
    ds << static_cast<qint16>(0) << static_cast<qint16>(0); // Grid X, Y (offset 36..39)
    ds << static_cast<quint16>(16) << static_cast<quint16>(16); // Grid W, H (offset 40..43)
    for (int i = 0; i < 84; ++i) {
        ds << static_cast<quint8>(0);       // Reserved (offset 44..127)
    }

    // Prepare animations for Frame Tags chunk (written in Frame 0)
    QList<SpriteAnimation> animList = inDoc.animations().values();
    bool hasTags = !animList.isEmpty();

    auto writeCompressedCel = [&ds](int layerIdx, int x, int y, int opacity, const QImage &img) {
        QImage cImg = img;
        if (cImg.format() != QImage::Format_ARGB32_Premultiplied) {
            cImg = cImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        int fw = cImg.width();
        int fh = cImg.height();
        QByteArray rawPixels;
        rawPixels.reserve(fw * fh * 4);
        for (int py = 0; py < fh; ++py) {
            const QRgb *scan = reinterpret_cast<const QRgb*>(cImg.constScanLine(py));
            for (int px = 0; px < fw; ++px) {
                rawPixels.append(static_cast<char>(qRed(scan[px])));
                rawPixels.append(static_cast<char>(qGreen(scan[px])));
                rawPixels.append(static_cast<char>(qBlue(scan[px])));
                rawPixels.append(static_cast<char>(qAlpha(scan[px])));
            }
        }
        QByteArray qComp = qCompress(rawPixels);
        QByteArray zlibData = (qComp.size() > 4) ? qComp.mid(4) : rawPixels;

        quint32 celChunkSize = 6 + 20 + zlibData.size();
        ds << static_cast<quint32>(celChunkSize);
        ds << static_cast<quint16>(0x2005);
        ds << static_cast<quint16>(layerIdx);
        ds << static_cast<qint16>(x);
        ds << static_cast<qint16>(y);
        ds << static_cast<quint8>(opacity);
        ds << static_cast<quint16>(2); // Compressed
        for (int i = 0; i < 7; ++i) ds << static_cast<quint8>(0);
        ds << static_cast<quint16>(fw);
        ds << static_cast<quint16>(fh);
        ds.writeRawData(zlibData.constData(), zlibData.size());
    };

    // 2. Frames
    for (int f = 0; f < numFrames; ++f) {
        int frameStart = fileBuf.size();

        bool hasDocCels = inDoc.hasFrameCels(f) && !inDoc.frameCels(f).isEmpty();
        quint16 celCount = hasDocCels ? static_cast<quint16>(inDoc.frameCels(f).size()) : 1;
        quint16 layerCount = (f == 0) ? (inDoc.hasLayers() ? static_cast<quint16>(inDoc.layers().size()) : 1) : 0;
        quint16 chunksCount = layerCount + celCount + ((f == 0 && hasTags) ? 1 : 0);

        // Frame header (16 bytes)
        ds << static_cast<quint32>(0);          // Frame bytes placeholder
        ds << static_cast<quint16>(0xF1FA);     // Magic
        ds << static_cast<quint16>(chunksCount);// Chunks count
        ds << static_cast<quint16>(frameDurationMs); // Duration ms
        ds << static_cast<quint8>(0) << static_cast<quint8>(0); // Reserved
        ds << static_cast<quint32>(0);          // New chunks count (if old == 0xFFFF)

        // Frame 0: Layer chunks (0x2004)
        if (f == 0) {
            if (inDoc.hasLayers()) {
                for (const SpriteLayer &sl : inDoc.layers()) {
                    QByteArray layerName = sl.name.toUtf8();
                    quint16 nameLen = static_cast<quint16>(layerName.size());
                    quint32 layerChunkSize = 6 + 18 + nameLen;
                    quint16 flags = (sl.visible ? 1 : 0) | (sl.locked ? 0 : 2);
                    quint16 lyrType = (sl.type == SpriteLayer::Group) ? 1 : (sl.type == SpriteLayer::Tilemap ? 2 : 0);

                    quint16 aseBlend = 0;
                    switch (sl.blendMode) {
                    case QPainter::CompositionMode_Multiply: aseBlend = 1; break;
                    case QPainter::CompositionMode_Screen: aseBlend = 2; break;
                    case QPainter::CompositionMode_Overlay: aseBlend = 3; break;
                    case QPainter::CompositionMode_Darken: aseBlend = 4; break;
                    case QPainter::CompositionMode_Lighten: aseBlend = 5; break;
                    case QPainter::CompositionMode_ColorDodge: aseBlend = 6; break;
                    case QPainter::CompositionMode_ColorBurn: aseBlend = 7; break;
                    case QPainter::CompositionMode_HardLight: aseBlend = 8; break;
                    case QPainter::CompositionMode_SoftLight: aseBlend = 9; break;
                    case QPainter::CompositionMode_Difference: aseBlend = 10; break;
                    case QPainter::CompositionMode_Exclusion: aseBlend = 11; break;
                    case QPainter::CompositionMode_Plus: aseBlend = 16; break;
                    default: aseBlend = 0; break;
                    }

                    ds << static_cast<quint32>(layerChunkSize);
                    ds << static_cast<quint16>(0x2004);
                    ds << static_cast<quint16>(flags);
                    ds << static_cast<quint16>(lyrType);
                    ds << static_cast<quint16>(sl.childLevel);
                    ds << static_cast<quint16>(canvasW);
                    ds << static_cast<quint16>(canvasH);
                    ds << static_cast<quint16>(aseBlend);
                    ds << static_cast<quint8>(sl.opacity);
                    ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0);
                    ds << static_cast<quint16>(nameLen);
                    ds.writeRawData(layerName.constData(), layerName.size());
                }
            } else {
                QByteArray layerName = "Layer 1";
                quint16 nameLen = static_cast<quint16>(layerName.size());
                quint32 layerChunkSize = 6 + 18 + nameLen;

                ds << static_cast<quint32>(layerChunkSize);
                ds << static_cast<quint16>(0x2004); // Chunk type
                ds << static_cast<quint16>(3);      // Flags: 1=visible | 2=editable
                ds << static_cast<quint16>(0);      // Layer type: 0=normal
                ds << static_cast<quint16>(0);      // Child level
                ds << static_cast<quint16>(canvasW);// Default width
                ds << static_cast<quint16>(canvasH);// Default height
                ds << static_cast<quint16>(0);      // Blend mode: 0=normal
                ds << static_cast<quint8>(255);     // Opacity
                ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0); // Reserved
                ds << static_cast<quint16>(nameLen);
                ds.writeRawData(layerName.constData(), layerName.size());
            }
        }

        // Cel chunks (0x2005)
        if (hasDocCels) {
            for (const SpriteCel &sc : inDoc.frameCels(f)) {
                writeCompressedCel(sc.layerIndex, sc.x, sc.y, sc.opacity, sc.image);
            }
        } else {
            writeCompressedCel(0, 0, 0, 255, inDoc.frame(f));
        }

        // Frame 0: Optional Frame Tags chunk (0x2018)
        if (f == 0 && hasTags) {
            quint16 numTags = static_cast<quint16>(animList.size());
            quint32 totalTagBodySize = 0;
            for (const SpriteAnimation &anim : animList) {
                totalTagBodySize += 19 + anim.name.toUtf8().size();
            }
            quint32 tagsChunkSize = 6 + 10 + totalTagBodySize;

            ds << static_cast<quint32>(tagsChunkSize);
            ds << static_cast<quint16>(0x2018); // Chunk type
            ds << static_cast<quint16>(numTags);// Number of tags
            for (int i = 0; i < 8; ++i) ds << static_cast<quint8>(0); // Reserved

            for (const SpriteAnimation &anim : animList) {
                QByteArray tagUtf8 = anim.name.toUtf8();
                quint16 fromF = 0;
                quint16 toF = static_cast<quint16>(qMax(0, numFrames - 1));
                if (!anim.frameIndices.isEmpty()) {
                    fromF = static_cast<quint16>(anim.frameIndices.first());
                    toF = static_cast<quint16>(anim.frameIndices.last());
                }

                quint8 loopDir = 0; // Forward
                if (anim.loopMode == SpriteAnimation::PingPong) {
                    loopDir = 2; // Ping-pong
                }

                ds << static_cast<quint16>(fromF);
                ds << static_cast<quint16>(toF);
                ds << static_cast<quint8>(loopDir);
                ds << static_cast<quint16>(0); // Repeat count (0=infinite)
                for (int i = 0; i < 6; ++i) ds << static_cast<quint8>(0); // Reserved
                ds << static_cast<quint8>(0) << static_cast<quint8>(0) << static_cast<quint8>(0); // RGB
                ds << static_cast<quint8>(0);  // Extra zero
                ds << static_cast<quint16>(tagUtf8.size());
                for (char c : tagUtf8) {
                    ds << static_cast<quint8>(c);
                }
            }
        }

        // Patch frame size at frameStart
        quint32 frameBytes = static_cast<quint32>(fileBuf.size() - frameStart);
        std::memcpy(fileBuf.data() + frameStart, &frameBytes, sizeof(quint32));

        setProgress(10 + (f * 80 / numFrames));
    }

    // Patch total file size at offset 0
    quint32 totalSize = static_cast<quint32>(fileBuf.size());
    std::memcpy(fileBuf.data(), &totalSize, sizeof(quint32));

    // Save to disk
    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot create output Aseprite file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    outFile.write(fileBuf);
    outFile.close();

    setProgress(100);
    setStatusMessage(tr("Exported Aseprite project successfully (%1 frames).").arg(numFrames));
    if (error) error->code = ExtractorError::NoError;
    return true;
}
