/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "packer/atlaspacker.h"
#include "packer/tightpolygonpacker.h"
#include "license/integrityguard.h"
#include <QPainter>
#include <cmath>
#include <cstring>
#include <algorithm>

int AtlasPacker::nextPowerOfTwo(int n)
{
    if (n <= 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

bool AtlasPacker::areImagesIdentical(const QImage &a, const QImage &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    if (a.isNull() && b.isNull()) {
        return true;
    }
    if (a.isNull() || b.isNull()) {
        return false;
    }

    QImage imgA = (a.format() == QImage::Format_ARGB32) ? a : a.convertToFormat(QImage::Format_ARGB32);
    QImage imgB = (b.format() == QImage::Format_ARGB32) ? b : b.convertToFormat(QImage::Format_ARGB32);

    int bytesPerLine = imgA.width() * 4;
    for (int y = 0; y < imgA.height(); ++y) {
        if (std::memcmp(imgA.constScanLine(y), imgB.constScanLine(y), bytesPerLine) != 0) {
            return false;
        }
    }

    return true;
}

void AtlasPacker::applyExtrusion(QImage &atlas, const QRect &targetRect, const QImage &sprite, int extrudeAmount)
{
    if (extrudeAmount <= 0) return;

    int w = sprite.width();
    int h = sprite.height();
    int tx = targetRect.x();
    int ty = targetRect.y();

    // Top and Bottom rows
    for (int x = 0; x < w; ++x) {
        QRgb topPixel = sprite.pixel(x, 0);
        QRgb bottomPixel = sprite.pixel(x, h - 1);
        for (int dy = 1; dy <= extrudeAmount; ++dy) {
            if (ty - dy >= 0 && tx + x < atlas.width()) {
                atlas.setPixel(tx + x, ty - dy, topPixel);
            }
            if (ty + h - 1 + dy < atlas.height() && tx + x < atlas.width()) {
                atlas.setPixel(tx + x, ty + h - 1 + dy, bottomPixel);
            }
        }
    }

    // Left and Right columns
    for (int y = 0; y < h; ++y) {
        QRgb leftPixel = sprite.pixel(0, y);
        QRgb rightPixel = sprite.pixel(w - 1, y);
        for (int dx = 1; dx <= extrudeAmount; ++dx) {
            if (tx - dx >= 0 && ty + y < atlas.height()) {
                atlas.setPixel(tx - dx, ty + y, leftPixel);
            }
            if (tx + w - 1 + dx < atlas.width() && ty + y < atlas.height()) {
                atlas.setPixel(tx + w - 1 + dx, ty + y, rightPixel);
            }
        }
    }

    // 4 Corners
    QRgb topLeft = sprite.pixel(0, 0);
    QRgb topRight = sprite.pixel(w - 1, 0);
    QRgb bottomLeft = sprite.pixel(0, h - 1);
    QRgb bottomRight = sprite.pixel(w - 1, h - 1);

    for (int dx = 1; dx <= extrudeAmount; ++dx) {
        for (int dy = 1; dy <= extrudeAmount; ++dy) {
            if (tx - dx >= 0 && ty - dy >= 0) {
                atlas.setPixel(tx - dx, ty - dy, topLeft);
            }
            if (tx + w - 1 + dx < atlas.width() && ty - dy >= 0) {
                atlas.setPixel(tx + w - 1 + dx, ty - dy, topRight);
            }
            if (tx - dx >= 0 && ty + h - 1 + dy < atlas.height()) {
                atlas.setPixel(tx - dx, ty + h - 1 + dy, bottomLeft);
            }
            if (tx + w - 1 + dx < atlas.width() && ty + h - 1 + dy < atlas.height()) {
                atlas.setPixel(tx + w - 1 + dx, ty + h - 1 + dy, bottomRight);
            }
        }
    }
}

AtlasPackResult AtlasPacker::pack(const QList<QImage> &frames, const PackOptions &options, const QList<QPolygonF> &polygons)
{
    AtlasPackResult result;
    if (frames.isEmpty()) {
        return result;
    }

    // Deduplication step (Auto-Aliasing)
    QList<QImage> uniqueFrames;
    QList<QPolygonF> uniquePolygons;
    QList<int> mapping;
    mapping.reserve(frames.size());

    if (options.deduplicate) {
        for (int i = 0; i < frames.size(); ++i) {
            const QImage &img = frames.at(i);
            int matchedIndex = -1;

            for (int u = 0; u < uniqueFrames.size(); ++u) {
                if (areImagesIdentical(img, uniqueFrames.at(u))) {
                    matchedIndex = u;
                    break;
                }
            }

            if (matchedIndex >= 0) {
                mapping.append(matchedIndex);
            } else {
                mapping.append(uniqueFrames.size());
                uniqueFrames.append(img);
                if (i < polygons.size()) {
                    uniquePolygons.append(polygons.at(i));
                } else {
                    uniquePolygons.append(QPolygonF());
                }
            }
        }
    } else {
        uniqueFrames = frames;
        uniquePolygons = polygons;
        for (int i = 0; i < frames.size(); ++i) {
            mapping.append(i);
        }
    }

    result.uniqueFramesCount = uniqueFrames.size();
    result.duplicateMapping = mapping;

    AtlasPackResult res;
    switch (options.algorithm) {
    case TightPolygon:
        res = TightPolygonPacker::pack(uniqueFrames, mapping, options, uniquePolygons);
        break;
    case GridPacker:
        res = packGrid(uniqueFrames, mapping, options);
        break;
    case RowPacker:
    case PowerOfTwoPacker: {
        PackOptions rowOpts = options;
        if (options.algorithm == PowerOfTwoPacker) {
            rowOpts.powerOfTwo = true;
        }
        res = packRow(uniqueFrames, mapping, rowOpts);
        break;
    }
    case MaxRects:
    default:
        res = packMaxRects(uniqueFrames, mapping, options);
        break;
    }

    if (res.success && !res.atlas.isNull()) {
        SpriteStudio::IntegrityGuard::applySteganographicWatermark(res.atlas);
    }
    return res;
}

AtlasPackResult AtlasPacker::pack(const QList<QImage> &frames, int padding, Algorithm algo)
{
    PackOptions options;
    options.algorithm = algo;
    options.padding = padding;
    options.borderPadding = padding;
    if (algo == PowerOfTwoPacker) {
        options.powerOfTwo = true;
    }
    return pack(frames, options);
}

AtlasPackResult AtlasPacker::packIndices(const QList<QImage> &allFrames, const QList<int> &frameIndices, int padding)
{
    PackOptions options;
    options.padding = padding;
    options.borderPadding = padding;
    options.algorithm = RowPacker;
    return packIndices(allFrames, frameIndices, options);
}

AtlasPackResult AtlasPacker::packIndices(const QList<QImage> &allFrames, const QList<int> &frameIndices, const PackOptions &options)
{
    AtlasPackResult result;
    if (frameIndices.isEmpty() || allFrames.isEmpty()) {
        return result;
    }

    QList<QImage> subset;
    subset.reserve(frameIndices.size());
    for (int idx : frameIndices) {
        if (idx >= 0 && idx < allFrames.size()) {
            subset.append(allFrames[idx]);
        }
    }

    return pack(subset, options);
}

AtlasPackResult AtlasPacker::packMaxRects(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options)
{
    AtlasPackResult result;
    result.uniqueFramesCount = uniqueFrames.size();
    result.duplicateMapping = mapping;

    int extrude = std::max(0, options.extrude);
    int pad = std::max(0, options.padding);
    int border = std::max(0, options.borderPadding);

    // Compute required allocation sizes per unique frame
    QList<QSize> allocSizes;
    allocSizes.reserve(uniqueFrames.size());

    long long totalArea = 0;
    int maxAllocW = 0;
    int maxAllocH = 0;

    for (const QImage &img : uniqueFrames) {
        int w = img.width() + extrude * 2 + pad;
        int h = img.height() + extrude * 2 + pad;
        allocSizes.append(QSize(w, h));
        totalArea += static_cast<long long>(w) * h;
        if (w > maxAllocW) maxAllocW = w;
        if (h > maxAllocH) maxAllocH = h;
    }

    // Determine target dimensions
    int binW = 0;
    int binH = 0;
    QList<QRect> placedRects;

    if (options.powerOfTwo) {
        int minW = std::max(maxAllocW + border * 2, 32);
        int minH = std::max(maxAllocH + border * 2, 32);
        binW = nextPowerOfTwo(minW);
        binH = nextPowerOfTwo(minH);

        while (static_cast<long long>(binW) * binH < totalArea * 1.05 && (binW < options.maxWidth || binH < options.maxHeight)) {
            if (options.forceSquare) {
                binW = std::min(binW * 2, options.maxWidth);
                binH = std::min(binH * 2, options.maxHeight);
            } else if (binW <= binH && binW < options.maxWidth) {
                binW *= 2;
            } else if (binH < options.maxHeight) {
                binH *= 2;
            } else if (binW < options.maxWidth) {
                binW *= 2;
            } else {
                break;
            }
        }

        bool fits = false;
        while (!fits && binW <= options.maxWidth && binH <= options.maxHeight) {
            ::MaxRectsPacker packer(binW - border * 2, binH - border * 2);
            placedRects = packer.insert(allocSizes, options.heuristic);

            fits = true;
            for (const QRect &r : placedRects) {
                if (r.width() == 0 || r.height() == 0) {
                    fits = false;
                    break;
                }
            }

            if (!fits) {
                if (options.forceSquare) {
                    binW = (binW < options.maxWidth) ? binW * 2 : options.maxWidth + 1;
                    binH = (binH < options.maxHeight) ? binH * 2 : options.maxHeight + 1;
                } else if (binW <= binH && binW < options.maxWidth) {
                    binW *= 2;
                } else if (binH < options.maxHeight) {
                    binH *= 2;
                } else if (binW < options.maxWidth) {
                    binW *= 2;
                } else {
                    break;
                }
            }
        }

        if (!fits) {
            return result;
        }
    } else {
        // Free-size / compact packing
        int sideEstimate = static_cast<int>(std::sqrt(totalArea * 1.15)) + border * 2;
        binW = std::max(sideEstimate, maxAllocW + border * 2);
        binH = binW;

        bool fits = false;
        int attempts = 0;
        while (!fits && attempts < 10 && binW <= options.maxWidth && binH <= options.maxHeight) {
            ::MaxRectsPacker packer(binW - border * 2, binH - border * 2);
            placedRects = packer.insert(allocSizes, options.heuristic);

            fits = true;
            for (const QRect &r : placedRects) {
                if (r.width() == 0 || r.height() == 0) {
                    fits = false;
                    break;
                }
            }

            if (!fits) {
                binW = static_cast<int>(binW * 1.25);
                binH = static_cast<int>(binH * 1.25);
                attempts++;
            }
        }

        if (!fits) {
            return result;
        }

        // Shrink-to-fit: calculate bounding box of placed items
        int maxUsedX = 0;
        int maxUsedY = 0;
        for (int i = 0; i < placedRects.size(); ++i) {
            const QRect &r = placedRects.at(i);
            const QImage &img = uniqueFrames.at(i);
            int rightEdge = r.x() + extrude * 2 + img.width();
            int bottomEdge = r.y() + extrude * 2 + img.height();
            if (rightEdge > maxUsedX) maxUsedX = rightEdge;
            if (bottomEdge > maxUsedY) maxUsedY = bottomEdge;
        }

        binW = maxUsedX + border * 2;
        binH = maxUsedY + border * 2;
        if (options.forceSquare) {
            int sq = std::max(binW, binH);
            binW = sq;
            binH = sq;
        }
    }

    // Render composite atlas
    QImage atlasImage(binW, binH, QImage::Format_ARGB32_Premultiplied);
    atlasImage.fill(SpriteStudio::IntegrityGuard::transparentBackgroundColor());

    QPainter painter(&atlasImage);
    QList<QRect> uniqueSpriteRects;
    uniqueSpriteRects.reserve(uniqueFrames.size());

    for (int i = 0; i < uniqueFrames.size(); ++i) {
        const QRect &placed = placedRects.at(i);
        const QImage &sprite = uniqueFrames.at(i);

        int spriteX = border + placed.x() + extrude;
        int spriteY = border + placed.y() + extrude;
        QRect spriteRect(spriteX, spriteY, sprite.width(), sprite.height());
        uniqueSpriteRects.append(spriteRect);

        painter.drawImage(spriteRect.topLeft(), sprite);
    }
    painter.end();

    // Extrusion pass
    if (extrude > 0) {
        for (int i = 0; i < uniqueFrames.size(); ++i) {
            applyExtrusion(atlasImage, uniqueSpriteRects.at(i), uniqueFrames.at(i), extrude);
        }
    }

    // Map rectangles back to all original frames
    QList<QRect> allFrameRects;
    allFrameRects.reserve(mapping.size());
    for (int origIdx = 0; origIdx < mapping.size(); ++origIdx) {
        int uIdx = mapping.at(origIdx);
        allFrameRects.append(uniqueSpriteRects.at(uIdx));
    }

    // Efficiency computation
    long long usefulArea = 0;
    for (const QImage &img : uniqueFrames) {
        usefulArea += static_cast<long long>(img.width()) * img.height();
    }
    float eff = (binW * binH > 0)
        ? (static_cast<float>(usefulArea) / static_cast<float>(binW * binH)) * 100.0f
        : 0.0f;

    result.atlas = atlasImage;
    result.frameRects = allFrameRects;
    result.dimensions = QSize(binW, binH);
    result.efficiency = eff;
    result.success = true;

    return result;
}

AtlasPackResult AtlasPacker::packRow(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options)
{
    AtlasPackResult result;
    result.uniqueFramesCount = uniqueFrames.size();
    result.duplicateMapping = mapping;

    int pad = std::max(0, options.padding);
    int border = std::max(0, options.borderPadding);
    int extrude = std::max(0, options.extrude);

    int totalArea = 0;
    int maxFrameW = 0;
    int maxFrameH = 0;

    for (const QImage &img : uniqueFrames) {
        int w = img.width() + extrude * 2 + pad;
        int h = img.height() + extrude * 2 + pad;
        totalArea += w * h;
        if (w > maxFrameW) maxFrameW = w;
        if (h > maxFrameH) maxFrameH = h;
    }

    int sideEstimate = static_cast<int>(std::sqrt(totalArea * 1.25)) + border * 2;
    int targetWidth = std::max(sideEstimate, maxFrameW + border * 2);
    if (options.powerOfTwo) {
        targetWidth = nextPowerOfTwo(targetWidth);
    }

    int currentX = border;
    int currentY = border;
    int rowHeight = 0;
    int maxUsedWidth = 0;

    QList<QRect> uniqueSpriteRects;
    uniqueSpriteRects.reserve(uniqueFrames.size());

    for (const QImage &img : uniqueFrames) {
        int itemW = img.width() + extrude * 2;
        int itemH = img.height() + extrude * 2;

        if (currentX + itemW + pad > targetWidth && currentX > border) {
            currentX = border;
            currentY += rowHeight + pad;
            rowHeight = 0;
        }

        QRect sRect(currentX + extrude, currentY + extrude, img.width(), img.height());
        uniqueSpriteRects.append(sRect);

        currentX += itemW + pad;
        if (currentX > maxUsedWidth) {
            maxUsedWidth = currentX;
        }
        if (itemH > rowHeight) {
            rowHeight = itemH;
        }
    }

    int finalWidth = options.powerOfTwo ? targetWidth : std::max(maxUsedWidth + border, maxFrameW + border * 2);
    int finalHeight = currentY + rowHeight + border;
    if (options.powerOfTwo) {
        finalHeight = nextPowerOfTwo(finalHeight);
    }
    if (options.forceSquare) {
        int sq = std::max(finalWidth, finalHeight);
        finalWidth = sq;
        finalHeight = sq;
    }

    QImage atlasImage(finalWidth, finalHeight, QImage::Format_ARGB32_Premultiplied);
    atlasImage.fill(SpriteStudio::IntegrityGuard::transparentBackgroundColor());

    QPainter painter(&atlasImage);
    for (int i = 0; i < uniqueFrames.size(); ++i) {
        painter.drawImage(uniqueSpriteRects.at(i).topLeft(), uniqueFrames.at(i));
    }
    painter.end();

    if (extrude > 0) {
        for (int i = 0; i < uniqueFrames.size(); ++i) {
            applyExtrusion(atlasImage, uniqueSpriteRects.at(i), uniqueFrames.at(i), extrude);
        }
    }

    QList<QRect> allFrameRects;
    allFrameRects.reserve(mapping.size());
    for (int origIdx = 0; origIdx < mapping.size(); ++origIdx) {
        allFrameRects.append(uniqueSpriteRects.at(mapping.at(origIdx)));
    }

    long long usefulArea = 0;
    for (const QImage &img : uniqueFrames) {
        usefulArea += static_cast<long long>(img.width()) * img.height();
    }
    float eff = (finalWidth * finalHeight > 0)
        ? (static_cast<float>(usefulArea) / static_cast<float>(finalWidth * finalHeight)) * 100.0f
        : 0.0f;

    result.atlas = atlasImage;
    result.frameRects = allFrameRects;
    result.dimensions = QSize(finalWidth, finalHeight);
    result.efficiency = eff;
    result.success = true;

    return result;
}

AtlasPackResult AtlasPacker::packGrid(const QList<QImage> &uniqueFrames, const QList<int> &mapping, const PackOptions &options)
{
    AtlasPackResult result;
    result.uniqueFramesCount = uniqueFrames.size();
    result.duplicateMapping = mapping;

    int pad = std::max(0, options.padding);
    int border = std::max(0, options.borderPadding);
    int extrude = std::max(0, options.extrude);

    int maxCellW = 0;
    int maxCellH = 0;
    for (const QImage &img : uniqueFrames) {
        if (img.width() > maxCellW) maxCellW = img.width();
        if (img.height() > maxCellH) maxCellH = img.height();
    }

    int cellW = maxCellW + extrude * 2;
    int cellH = maxCellH + extrude * 2;

    int cols = static_cast<int>(std::ceil(std::sqrt(uniqueFrames.size())));
    if (cols < 1) cols = 1;
    int rows = static_cast<int>(std::ceil(static_cast<double>(uniqueFrames.size()) / cols));

    int finalWidth = border * 2 + cols * cellW + (cols - 1) * pad;
    int finalHeight = border * 2 + rows * cellH + (rows - 1) * pad;

    if (options.powerOfTwo) {
        finalWidth = nextPowerOfTwo(finalWidth);
        finalHeight = nextPowerOfTwo(finalHeight);
    }
    if (options.forceSquare) {
        int sq = std::max(finalWidth, finalHeight);
        finalWidth = sq;
        finalHeight = sq;
    }

    QImage atlasImage(finalWidth, finalHeight, QImage::Format_ARGB32_Premultiplied);
    atlasImage.fill(SpriteStudio::IntegrityGuard::transparentBackgroundColor());

    QPainter painter(&atlasImage);
    QList<QRect> uniqueSpriteRects;
    uniqueSpriteRects.reserve(uniqueFrames.size());

    for (int i = 0; i < uniqueFrames.size(); ++i) {
        int r = i / cols;
        int c = i % cols;

        int cx = border + c * (cellW + pad);
        int cy = border + r * (cellH + pad);

        const QImage &sprite = uniqueFrames.at(i);
        // Center within cell
        int offsetX = (cellW - (sprite.width() + extrude * 2)) / 2 + extrude;
        int offsetY = (cellH - (sprite.height() + extrude * 2)) / 2 + extrude;

        QRect spriteRect(cx + offsetX, cy + offsetY, sprite.width(), sprite.height());
        uniqueSpriteRects.append(spriteRect);

        painter.drawImage(spriteRect.topLeft(), sprite);
    }
    painter.end();

    if (extrude > 0) {
        for (int i = 0; i < uniqueFrames.size(); ++i) {
            applyExtrusion(atlasImage, uniqueSpriteRects.at(i), uniqueFrames.at(i), extrude);
        }
    }

    QList<QRect> allFrameRects;
    allFrameRects.reserve(mapping.size());
    for (int origIdx = 0; origIdx < mapping.size(); ++origIdx) {
        allFrameRects.append(uniqueSpriteRects.at(mapping.at(origIdx)));
    }

    long long usefulArea = 0;
    for (const QImage &img : uniqueFrames) {
        usefulArea += static_cast<long long>(img.width()) * img.height();
    }
    float eff = (finalWidth * finalHeight > 0)
        ? (static_cast<float>(usefulArea) / static_cast<float>(finalWidth * finalHeight)) * 100.0f
        : 0.0f;

    result.atlas = atlasImage;
    result.frameRects = allFrameRects;
    result.dimensions = QSize(finalWidth, finalHeight);
    result.efficiency = eff;
    result.success = true;

    return result;
}
