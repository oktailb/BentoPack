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

#include "image/pixelhash.h"
#include "model/spritedocument.h"
#include <cstring>

#if __has_include(<xxhash.h>)
#  define XXH_INLINE_ALL
#  include <xxhash.h>
#  define HAVE_XXHASH 1
#endif

#if !defined(HAVE_XXHASH)
namespace {

// Portable 64-bit FNV-1a / Murmur mix fallback in case xxhash is absent
quint64 fallbackHash64(const void *data, size_t length, quint64 seed)
{
    const quint8 *ptr = reinterpret_cast<const quint8*>(data);
    quint64 h = 14695981039346656037ULL ^ seed;
    for (size_t i = 0; i < length; ++i) {
        h ^= static_cast<quint64>(ptr[i]);
        h *= 1099511628211ULL;
    }
    // Final avalanche mix
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

} // anonymous namespace
#endif

quint64 PixelHash::hashData(const void *data, size_t length, quint64 seed)
{
    if (!data || length == 0) {
        return seed;
    }
#if defined(HAVE_XXHASH)
    return XXH3_64bits_withSeed(data, length, seed);
#else
    return fallbackHash64(data, length, seed);
#endif
}

quint64 PixelHash::hashImage(const QImage &image)
{
    if (image.isNull()) {
        return 0;
    }

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) {
        return 0;
    }

    // Header metadata: width, height
    struct ImageMeta {
        qint32 width;
        qint32 height;
    } meta = { w, h };

    quint64 seed = hashData(&meta, sizeof(meta), 0x9e3779b97f4a7c15ULL);

    // Fast-path: image is 32-bit ARGB
    QImage canonical;
    const QImage *imgPtr = &image;
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_ARGB32_Premultiplied) {
        canonical = image.convertToFormat(QImage::Format_ARGB32);
        imgPtr = &canonical;
    }

    const int lineBytes = w * 4;
    const int bpl = imgPtr->bytesPerLine();

    // If lines are tightly packed without stride padding, hash entire buffer in one call (50+ GB/s)
    if (bpl == lineBytes && imgPtr->constBits()) {
        return hashData(imgPtr->constBits(), static_cast<size_t>(lineBytes * h), seed);
    }

    // Otherwise hash scanline by scanline to skip trailing stride padding
    quint64 currentHash = seed;
    for (int y = 0; y < h; ++y) {
        currentHash = hashData(imgPtr->constScanLine(y), static_cast<size_t>(lineBytes), currentHash);
    }
    return currentHash;
}

quint64 PixelHash::hashCel(const SpriteCel &cel)
{
    if (cel.isNull()) {
        return 0;
    }

    quint64 imgHash = hashImage(cel.image);

    struct CelMeta {
        qint32 layerIndex;
        qint16 x;
        qint16 y;
        quint8 opacity;
        quint8 pad;
        quint64 imageHash;
    } meta = {
        cel.layerIndex,
        cel.x,
        cel.y,
        cel.opacity,
        0,
        imgHash
    };

    quint64 h = hashData(&meta, sizeof(meta), 0xBF58476D1CE4E5B9ULL);

    if (!cel.layerId.isEmpty()) {
        QByteArray idUtf8 = cel.layerId.toUtf8();
        h = hashData(idUtf8.constData(), idUtf8.size(), h);
    }

    return h;
}

bool PixelHash::areImagesIdentical(const QImage &a, const QImage &b)
{
    if (a.isNull() && b.isNull()) return true;
    if (a.isNull() || b.isNull()) return false;
    if (a.size() != b.size()) return false;

    // Fast hash-check
    if (hashImage(a) != hashImage(b)) {
        return false;
    }

    // Verify byte-for-byte to prevent any hypothetical hash collisions
    QImage imgA = (a.format() == QImage::Format_ARGB32) ? a : a.convertToFormat(QImage::Format_ARGB32);
    QImage imgB = (b.format() == QImage::Format_ARGB32) ? b : b.convertToFormat(QImage::Format_ARGB32);

    const int bytesPerLine = imgA.width() * 4;
    for (int y = 0; y < imgA.height(); ++y) {
        if (std::memcmp(imgA.constScanLine(y), imgB.constScanLine(y), bytesPerLine) != 0) {
            return false;
        }
    }
    return true;
}

bool PixelHash::areCelsIdentical(const SpriteCel &a, const SpriteCel &b)
{
    if (a.isNull() && b.isNull()) return true;
    if (a.isNull() || b.isNull()) return false;

    if (a.layerIndex != b.layerIndex || a.layerId != b.layerId) return false;
    if (a.x != b.x || a.y != b.y || a.opacity != b.opacity) return false;

    return areImagesIdentical(a.image, b.image);
}

QRect PixelHash::computeDirtyRect(const QImage &a, const QImage &b)
{
    if (a.isNull() || b.isNull() || a.size() != b.size()) {
        int w = std::max(a.width(), b.width());
        int h = std::max(a.height(), b.height());
        return (w > 0 && h > 0) ? QRect(0, 0, w, h) : QRect();
    }

    if (areImagesIdentical(a, b)) {
        return QRect();
    }

    const int w = a.width();
    const int h = a.height();

    const QImage imgA = (a.format() == QImage::Format_ARGB32) ? a : a.convertToFormat(QImage::Format_ARGB32);
    const QImage imgB = (b.format() == QImage::Format_ARGB32) ? b : b.convertToFormat(QImage::Format_ARGB32);

    int minX = w, maxX = -1;
    int minY = h, maxY = -1;

    const int bytesPerLine = w * static_cast<int>(sizeof(QRgb));

    for (int y = 0; y < h; ++y) {
        const QRgb *lineA = reinterpret_cast<const QRgb*>(imgA.constScanLine(y));
        const QRgb *lineB = reinterpret_cast<const QRgb*>(imgB.constScanLine(y));

        if (std::memcmp(lineA, lineB, bytesPerLine) == 0) {
            continue;
        }

        for (int x = 0; x < w; ++x) {
            if (lineA[x] != lineB[x]) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect();
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}
