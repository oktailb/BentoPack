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

#include "image/retrolinterhelper.h"
#include "image/pixelhash.h"
#include <QSet>
#include <cstring>

int RetroLinterHelper::countUniqueTiles(const QList<QImage> &frames, int tileSize)
{
    if (tileSize <= 0) tileSize = 8;
    QSet<quint64> uniqueHashes;

    for (const QImage &frame : frames) {
        if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0) continue;

        QImage img = frame.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int w = img.width();
        int h = img.height();

        QByteArray tileBuffer;
        tileBuffer.resize(tileSize * tileSize * 4);

        for (int y = 0; y < h; y += tileSize) {
            for (int x = 0; x < w; x += tileSize) {
                bool hasOpaquePixel = false;
                char *bufPtr = tileBuffer.data();

                for (int ty = 0; ty < tileSize; ++ty) {
                    int py = y + ty;
                    for (int tx = 0; tx < tileSize; ++tx) {
                        int px = x + tx;
                        QRgb pixel = 0;
                        if (px < w && py < h) {
                            pixel = img.pixel(px, py);
                            if (qAlpha(pixel) > 0) {
                                hasOpaquePixel = true;
                            }
                        }
                        std::memcpy(bufPtr, &pixel, 4);
                        bufPtr += 4;
                    }
                }

                if (hasOpaquePixel) {
                    quint64 hash = PixelHash::hashData(tileBuffer.constData(), static_cast<size_t>(tileBuffer.size()));
                    uniqueHashes.insert(hash);
                }
            }
        }
    }

    return static_cast<int>(uniqueHashes.size());
}

bool RetroLinterHelper::isScanlineViolated(const QImage &frame, const RetroLinterConfig &cfg)
{
    if (!cfg.enabled || frame.isNull()) return false;
    return (frame.width() > cfg.scanlineSpriteWidthLimit);
}

QString RetroLinterHelper::scanlineViolationMessage(const QImage &frame, const RetroLinterConfig &cfg)
{
    if (!isScanlineViolated(frame, cfg)) return QString();
    return QStringLiteral("⚠️ Hardware warning: frame width (%1 px) exceeds target scanline limit (%2 px).")
        .arg(frame.width())
        .arg(cfg.scanlineSpriteWidthLimit);
}
