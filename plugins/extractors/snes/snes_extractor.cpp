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

#include "snes_extractor.h"
#include "snes_dialog.h"
#include "image/spritedetector.h"
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QWidget>

SnesExtractor::SnesExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool SnesExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // Check extensions
    if (supportedExtensions().contains(ext)) {
        return true;
    }

    // Inspect file contents
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(1024);
        file.close();
        if (SnesSavestateLoader::isSavestate(header, filePath)) {
            return true;
        }
    }
    return false;
}

static bool encodeSnes4bppTiles(const QImage &image, QByteArray &outData, QString *errorMsg = nullptr)
{
    if (image.isNull()) {
        if (errorMsg) *errorMsg = QStringLiteral("Cannot encode null image to SNES 4bpp tiles.");
        return false;
    }

    int tSize = 8;
    int numTilesX = image.width() / tSize;
    int numTilesY = image.height() / tSize;

    if (numTilesX <= 0 || numTilesY <= 0) {
        if (errorMsg) *errorMsg = QStringLiteral("Image dimensions smaller than a single 8x8 tile.");
        return false;
    }

    QImage indexed = image;
    if (indexed.format() != QImage::Format_Indexed8) {
        indexed = image.convertToFormat(QImage::Format_Indexed8, Qt::AutoColor);
    }

    const int bpt = 32;
    outData.clear();
    outData.reserve(numTilesX * numTilesY * bpt);

    QByteArray tileBuf(bpt, 0);

    for (int ty = 0; ty < numTilesY; ++ty) {
        for (int tx = 0; tx < numTilesX; ++tx) {
            tileBuf.fill(0);
            int originX = tx * tSize;
            int originY = ty * tSize;

            for (int y = 0; y < 8; ++y) {
                uchar b0 = 0, b1 = 0, b2 = 0, b3 = 0;
                const uchar *line = indexed.constScanLine(originY + y) + originX;
                for (int x = 0; x < 8; ++x) {
                    uchar idx = line[x] % 16;
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
            outData.append(tileBuf);
        }
    }
    return true;
}

bool SnesExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open SNES file: %1").arg(filePath);
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

    QImage atlas;
    QList<QRect> spriteBoxes;
    bool extractAsSprites = true;

    if (SnesSavestateLoader::isSavestate(data, filePath)) {
        SnesSavestateData state = SnesSavestateLoader::loadFromData(data, filePath);
        QVector<QRgb> pal = state.getPalette(8, true); // OBJ palette 0
        atlas = RetroTileDecoder::decodeTileSheet(
            reinterpret_cast<const uchar*>(state.vram.constData()),
            state.vram.size(),
            RetroFormat::Snes4bpp,
            16,
            2048,
            pal,
            true,
            TilePattern::Sprite16x16_2x2
        );
    } else {
        // Check presets first
        bool hasHeader = SnesAddress::hasCopierHeader(data.size());
        bool isHi = SnesAddress::detectHiRom(data, hasHeader);
        QString title = SnesAddress::readInternalRomTitle(data, hasHeader, isHi);

        QList<SnesPreset> presets = SnesPresetManager::builtInPresets();
        SnesPreset matchedPreset;
        for (const auto &p : presets) {
            if (!title.isEmpty() && (title.contains(p.gameName, Qt::CaseInsensitive) || p.gameName.contains(title, Qt::CaseInsensitive))) {
                matchedPreset = p;
                break;
            }
        }

        if (matchedPreset.isValid()) {
            QVector<QRgb> pal;
            if (matchedPreset.paletteOffset >= 0 && matchedPreset.paletteOffset + 32 <= data.size()) {
                pal.resize(16);
                const uchar *pRaw = reinterpret_cast<const uchar*>(data.constData()) + matchedPreset.paletteOffset;
                for (int i = 0; i < 16; ++i) {
                    quint16 w = qFromLittleEndian<quint16>(pRaw + (i * 2));
                    pal[i] = SnesSavestateData::bgr555ToRgb(w, (i == 0) ? 0 : 255);
                }
            } else {
                pal = RetroTileDecoder::defaultPreviewPalette(RetroFormat::Snes4bpp);
            }
            QByteArray slice = data.mid(matchedPreset.tileOffset, matchedPreset.tileLength);
            atlas = RetroTileDecoder::decodeTileSheet(
                reinterpret_cast<const uchar*>(slice.constData()),
                slice.size(),
                RetroFormat::Snes4bpp,
                matchedPreset.tilesPerRow,
                4096,
                pal,
                true,
                matchedPreset.pattern
            );
        } else {
            // Default probe decoding
            QVector<QRgb> pal = RetroTileDecoder::defaultPreviewPalette(RetroFormat::Snes4bpp);
            int off = hasHeader ? 512 : 0;
            atlas = RetroTileDecoder::decodeTileSheet(
                reinterpret_cast<const uchar*>(data.constData()) + off,
                data.size() - off,
                RetroFormat::Snes4bpp,
                16,
                2048,
                pal,
                true,
                TilePattern::Linear
            );
        }
    }

    QList<SpriteBox> detected;
    SpriteDetectionOptions detOpts;
    detOpts.alphaThreshold = 1;
    detOpts.minSliceSize = 4;
    if (SpriteDetector::detectBoxes(atlas, detected, detOpts)) {
        for (const SpriteBox &sb : detected) {
            spriteBoxes.append(sb.rect);
        }
    }

    if (atlas.isNull()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("Failed to decode SNES graphics.");
            error->filePath = filePath;
        }
        return false;
    }

    QList<QImage> frames;
    QList<SpriteBox> boxes;

    if (extractAsSprites && !spriteBoxes.isEmpty()) {
        for (const QRect &box : spriteBoxes) {
            if (box.width() >= 4 && box.height() >= 4 && atlas.rect().contains(box)) {
                frames.append(atlas.copy(box));
                boxes.append(SpriteBox(box));
            }
        }
    }

    if (frames.isEmpty()) {
        frames.append(atlas);
        boxes.append(SpriteBox(QRect(0, 0, atlas.width(), atlas.height())));
    }

    outDoc.setAtlas(atlas);
    outDoc.setFrames(frames, boxes);
    outDoc.setFilePath(filePath);

    return true;
}

bool SnesExtractor::write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error)
{
    Q_UNUSED(options);
    QImage atlas = inDoc.atlas();
    if (atlas.isNull()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot export SNES: Document atlas image is null.");
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray outTiles;
    QString err;
    if (!encodeSnes4bppTiles(atlas, outTiles, &err)) {
        if (error) {
            error->code = ExtractorError::UnsupportedFormat;
            error->message = tr("Failed to encode SNES 4bpp tiles: %1").arg(err);
            error->filePath = filePath;
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Cannot open file for writing: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    if (file.write(outTiles) != outTiles.size()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to write all tile bytes to disk.");
            error->filePath = filePath;
        }
        return false;
    }

    return true;
}

bool SnesExtractor::showInteractiveDialog(const QString &filePath, SpriteDocument &outDoc, QWidget *parent)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    SnesDialog dialog(filePath, data, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QImage atlas = dialog.decodedAtlasImage();
    if (atlas.isNull()) {
        return false;
    }

    QList<QRect> spriteBoxes = dialog.detectedSpriteBoxes();
    bool extractAsSprites = dialog.extractAsSprites();

    QList<QImage> frames;
    QList<SpriteBox> boxes;

    if (extractAsSprites && !spriteBoxes.isEmpty()) {
        for (const QRect &box : spriteBoxes) {
            if (box.width() >= 4 && box.height() >= 4 && atlas.rect().contains(box)) {
                frames.append(atlas.copy(box));
                boxes.append(SpriteBox(box));
            }
        }
    }

    if (frames.isEmpty()) {
        frames.append(atlas);
        boxes.append(SpriteBox(QRect(0, 0, atlas.width(), atlas.height())));
    }

    outDoc.setAtlas(atlas);
    outDoc.setFrames(frames, boxes);
    outDoc.setFilePath(filePath);
    return true;
}
