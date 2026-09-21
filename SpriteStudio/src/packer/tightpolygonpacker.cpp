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

#include "packer/tightpolygonpacker.h"
#include <QPainter>
#include <QPainterPath>
#include <QtConcurrent>
#include <QThreadPool>
#include <cmath>
#include <algorithm>
#include <vector>

namespace {

struct Span {
    int y;
    int x1;
    int x2;
};

struct SpriteToPack {
    int origIndex;
    QImage image;
    QPolygonF polygon;
    int w;
    int h;
    std::vector<uint8_t> mask; // 1 if occupied (including padding), 0 otherwise
    std::vector<Span> spans;   // Horizontal runs of occupied pixels for fast collision
    int occupiedArea = 0;      // Number of pixels in dilated mask
};

int nextPowerOfTwo(int n)
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

// Build 1-byte-per-pixel occupancy mask dilated with padding
std::vector<uint8_t> createDilatedMask(const QImage &img, const QPolygonF &poly, int pad)
{
    const int w = img.width();
    const int h = img.height();
    std::vector<uint8_t> rawMask(w * h, 0);

    // 1. Mark pixels inside polygon or alpha > 5
    if (!poly.isEmpty() && poly.size() >= 3) {
        QImage pImg(w, h, QImage::Format_ARGB32_Premultiplied);
        pImg.fill(Qt::transparent);
        QPainter painter(&pImg);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
        painter.end();

        for (int y = 0; y < h; ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(pImg.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                if (qAlpha(line[x]) > 127) {
                    rawMask[y * w + x] = 1;
                }
            }
        }
    } else {
        // Fallback to alpha mask of image
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (qAlpha(img.pixel(x, y)) > 5) {
                    rawMask[y * w + x] = 1;
                }
            }
        }
    }

    if (pad <= 0) {
        return rawMask;
    }

    // 2. Dilate mask by pad pixels (Euclidean / Chebyshev distance)
    std::vector<uint8_t> dilated = rawMask;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (rawMask[y * w + x] == 0) continue;
            // Spread pad radius
            int yMin = std::max(0, y - pad);
            int yMax = std::min(h - 1, y + pad);
            int xMin = std::max(0, x - pad);
            int xMax = std::min(w - 1, x + pad);
            for (int dy = yMin; dy <= yMax; ++dy) {
                for (int dx = xMin; dx <= xMax; ++dx) {
                    dilated[dy * w + dx] = 1;
                }
            }
        }
    }
    return dilated;
}

void buildSpans(SpriteToPack &s)
{
    s.spans.clear();
    s.occupiedArea = 0;
    for (int y = 0; y < s.h; ++y) {
        int x = 0;
        const uint8_t *row = s.mask.data() + y * s.w;
        while (x < s.w) {
            if (row[x]) {
                int xStart = x;
                while (x < s.w && row[x]) {
                    x++;
                }
                int xEnd = x - 1;
                s.spans.push_back({ y, xStart, xEnd });
                s.occupiedArea += (xEnd - xStart + 1);
            } else {
                x++;
            }
        }
    }
}

inline bool checkCollision(const std::vector<uint8_t> &grid, int binW, int binH,
                           const SpriteToPack &sprite, int posX, int posY)
{
    const int sw = sprite.w;
    const int sh = sprite.h;
    if (posX < 0 || posY < 0 || posX + sw > binW || posY + sh > binH) {
        return true;
    }

    const uint8_t *gridData = grid.data();
    for (const Span &span : sprite.spans) {
        const uint8_t *gRow = gridData + (posY + span.y) * binW + posX;
        for (int x = span.x1; x <= span.x2; ++x) {
            if (gRow[x]) {
                return true; // Collision detected
            }
        }
    }
    return false;
}

inline void stampMask(std::vector<uint8_t> &grid, int binW,
                      const SpriteToPack &sprite, int posX, int posY)
{
    uint8_t *gridData = grid.data();
    for (const Span &span : sprite.spans) {
        uint8_t *gRow = gridData + (posY + span.y) * binW + posX;
        for (int x = span.x1; x <= span.x2; ++x) {
            gRow[x] = 1;
        }
    }
}

// Multi-step adaptive sliding towards top-left to snuggly settle into concave crevices
void slidePosition(const std::vector<uint8_t> &grid, int binW, int binH,
                   const SpriteToPack &sprite, int border, int &posX, int &posY)
{
    bool moved = true;
    int iter = 0;
    while (moved && iter < 8) {
        moved = false;
        iter++;

        // Fast exponential slide left
        for (int step : { 64, 32, 16, 8, 4, 2, 1 }) {
            while (posX - step >= border && !checkCollision(grid, binW, binH, sprite, posX - step, posY)) {
                posX -= step;
                moved = true;
            }
        }

        // Fast exponential slide up
        for (int step : { 64, 32, 16, 8, 4, 2, 1 }) {
            while (posY - step >= border && !checkCollision(grid, binW, binH, sprite, posX, posY - step)) {
                posY -= step;
                moved = true;
            }
        }
    }
}

inline long long evaluatePlacementScore(int testX, int testY, int spriteW, int spriteH, int border,
                                       int currentUsedW, int currentUsedH,
                                       bool forceSquare)
{
    int newW = std::max(currentUsedW, testX + spriteW + border);
    int newH = std::max(currentUsedH, testY + spriteH + border);

    long long currentArea = static_cast<long long>(currentUsedW) * currentUsedH;
    long long newArea = static_cast<long long>(newW) * newH;
    long long deltaArea = newArea - currentArea;

    // Heavily penalize increasing the global atlas bounding box area.
    // This prioritizes filling existing pockets and holes (where deltaArea == 0).
    long long score = deltaArea * 10000;

    if (forceSquare) {
        int maxDim = std::max(newW, newH);
        score += static_cast<long long>(maxDim) * maxDim * 5000;
    } else {
        // Keep atlas aspect ratio relatively balanced (around 1:1)
        score += std::abs(newW - newH) * 40;
    }

    // Packing gravity: favor upper-left
    score += static_cast<long long>(testY) * 1000 + testX;
    return score;
}

struct PlacedSprite {
    int x, y, w, h;
};

} // namespace

AtlasPackResult TightPolygonPacker::pack(const QList<QImage> &uniqueFrames,
                                        const QList<int> &mapping,
                                        const AtlasPacker::PackOptions &options,
                                        const QList<QPolygonF> &uniquePolygons)
{
    AtlasPackResult result;
    if (uniqueFrames.isEmpty()) {
        return result;
    }

    result.uniqueFramesCount = uniqueFrames.size();
    result.duplicateMapping = mapping;

    const int pad = std::max(0, options.padding);
    const int border = std::max(0, options.borderPadding);
    const int extrude = std::max(0, options.extrude);
    const int effectivePad = pad + extrude * 2;

    const int hardwareCores = std::max(1, QThread::idealThreadCount());
    const int targetThreads = qBound(1, options.threadCount > 0 ? options.threadCount : hardwareCores, hardwareCores);

    QThreadPool threadPool;
    threadPool.setMaxThreadCount(targetThreads);

    // 1. Prepare sprites with dilated masks and span lists in parallel
    std::vector<SpriteToPack> sprites(uniqueFrames.size());
    QList<int> frameIndices;
    frameIndices.reserve(uniqueFrames.size());
    for (int i = 0; i < uniqueFrames.size(); ++i) {
        frameIndices.append(i);
    }

    QtConcurrent::blockingMap(&threadPool, frameIndices, [&](int i) {
        const QImage &img = uniqueFrames[i];
        QPolygonF poly = (i < uniquePolygons.size()) ? uniquePolygons[i] : QPolygonF();
        SpriteToPack &s = sprites[i];
        s.origIndex = i;
        s.image = img;
        s.polygon = poly;
        s.w = img.width();
        s.h = img.height();
        s.mask = createDilatedMask(img, poly, effectivePad);
        buildSpans(s);
    });

    long long totalOccupiedArea = 0;
    long long totalBoundingArea = 0;
    int maxW = 0, maxH = 0;
    for (const SpriteToPack &s : sprites) {
        totalOccupiedArea += s.occupiedArea;
        totalBoundingArea += static_cast<long long>(s.w) * s.h;
        if (s.w > maxW) maxW = s.w;
        if (s.h > maxH) maxH = s.h;
    }

    // 2. Sort sprites descending by max dimension and occupied area for optimal nesting
    std::sort(sprites.begin(), sprites.end(), [](const SpriteToPack &a, const SpriteToPack &b) {
        int valA = std::max(a.w, a.h) * 1000 + a.occupiedArea;
        int valB = std::max(b.w, b.h) * 1000 + b.occupiedArea;
        return valA > valB;
    });

    // 3. Estimate initial bin dimensions based on actual occupied content
    long long estTargetArea = std::max(totalOccupiedArea * 15 / 10, totalBoundingArea * 7 / 10);
    int estSide = static_cast<int>(std::sqrt(estTargetArea)) + border * 2;

    int binW = std::max(estSide, maxW + border * 2);
    int binH = std::max(estSide, maxH + border * 2);

    if (options.powerOfTwo) {
        binW = nextPowerOfTwo(binW);
        binH = nextPowerOfTwo(binH);
        while (static_cast<long long>(binW) * binH < estTargetArea &&
               (binW < options.maxWidth || binH < options.maxHeight)) {
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
    }

    // 4. Try packing loop with bin expansion if necessary
    bool allPacked = false;
    std::vector<QPoint> placements(sprites.size());

    while (!allPacked && binW <= options.maxWidth && binH <= options.maxHeight) {
        std::vector<uint8_t> grid(binW * binH, 0);
        allPacked = true;

        std::vector<PlacedSprite> placed;
        placed.reserve(sprites.size());

        int currentUsedW = border;
        int currentUsedH = border;

        for (size_t sIdx = 0; sIdx < sprites.size(); ++sIdx) {
            const SpriteToPack &sprite = sprites[sIdx];
            bool foundSpot = false;
            int bestX = -1, bestY = -1;
            long long bestScore = -1;

            // Generate candidate points
            std::vector<QPoint> candidates;
            candidates.push_back(QPoint(border, border));

            // Candidate positions derived from placed sprites
            for (const PlacedSprite &p : placed) {
                // Outer edges
                candidates.push_back(QPoint(p.x + p.w + pad, p.y));
                candidates.push_back(QPoint(p.x, p.y + p.h + pad));
                candidates.push_back(QPoint(p.x + p.w + pad, p.y + p.h + pad));
                candidates.push_back(QPoint(p.x + p.w + pad, border));
                candidates.push_back(QPoint(border, p.y + p.h + pad));

                // Interior pocket contact candidates (where frames can overlap)
                candidates.push_back(QPoint(p.x + p.w / 2, p.y + p.h + pad));
                candidates.push_back(QPoint(p.x + p.w + pad, p.y + p.h / 2));
                candidates.push_back(QPoint(p.x, p.y));
                candidates.push_back(QPoint(p.x + p.w / 2, p.y));
                candidates.push_back(QPoint(p.x, p.y + p.h / 2));
            }

            // Skyline sampling to catch gaps and pockets in the interior with coarse steps
            if (!placed.empty()) {
                std::vector<int> sampleY;
                sampleY.reserve(placed.size() * 2 + 1);
                sampleY.push_back(border);
                for (const PlacedSprite &p : placed) {
                    sampleY.push_back(p.y);
                    sampleY.push_back(p.y + p.h + pad);
                }
                std::sort(sampleY.begin(), sampleY.end());
                sampleY.erase(std::unique(sampleY.begin(), sampleY.end()), sampleY.end());

                for (int sy : sampleY) {
                    if (sy + sprite.h + border > binH) continue;
                    int maxXScan = std::min(binW - sprite.w - border, currentUsedW + pad);
                    // Step by 64px: slidePosition will slide up to 64px continuously to find obstacles
                    for (int sx = border; sx <= maxXScan; sx += 64) {
                        candidates.push_back(QPoint(sx, sy));
                    }
                }
            }

            // Filter out out-of-bounds candidates and deduplicate
            std::vector<QPoint> validCandidates;
            validCandidates.reserve(candidates.size());
            for (const QPoint &pt : candidates) {
                if (pt.x() >= border && pt.y() >= border &&
                    pt.x() + sprite.w + border <= binW &&
                    pt.y() + sprite.h + border <= binH) {
                    validCandidates.push_back(pt);
                }
            }
            std::sort(validCandidates.begin(), validCandidates.end(), [](const QPoint &a, const QPoint &b) {
                if (a.y() != b.y()) return a.y() < b.y();
                return a.x() < b.x();
            });
            validCandidates.erase(std::unique(validCandidates.begin(), validCandidates.end()), validCandidates.end());

            // Test each candidate in parallel if there are multiple candidates
            const int candidateCount = static_cast<int>(validCandidates.size());

            if (candidateCount >= 16 && targetThreads > 1) {
                struct ThreadCandidateResult {
                    int bestX = -1;
                    int bestY = -1;
                    long long bestScore = -1;
                };

                const int numChunks = std::min(targetThreads, candidateCount);
                QList<int> chunkIndices;
                chunkIndices.reserve(numChunks);
                for (int c = 0; c < numChunks; ++c) {
                    chunkIndices.append(c);
                }

                std::vector<ThreadCandidateResult> chunkResults(numChunks);

                QtConcurrent::blockingMap(&threadPool, chunkIndices, [&](int chunk) {
                    int startIdx = (chunk * candidateCount) / numChunks;
                    int endIdx = ((chunk + 1) * candidateCount) / numChunks;
                    ThreadCandidateResult &res = chunkResults[chunk];

                    for (int i = startIdx; i < endIdx; ++i) {
                        const QPoint &pt = validCandidates[i];
                        int testX = pt.x();
                        int testY = pt.y();
                        if (!checkCollision(grid, binW, binH, sprite, testX, testY)) {
                            slidePosition(grid, binW, binH, sprite, border, testX, testY);
                            long long score = evaluatePlacementScore(testX, testY, sprite.w, sprite.h, border,
                                                                     currentUsedW, currentUsedH,
                                                                     options.forceSquare);
                            if (res.bestScore < 0 || score < res.bestScore) {
                                res.bestScore = score;
                                res.bestX = testX;
                                res.bestY = testY;
                            }
                        }
                    }
                });

                for (const auto &cr : chunkResults) {
                    if (cr.bestScore >= 0 && (bestScore < 0 || cr.bestScore < bestScore)) {
                        bestScore = cr.bestScore;
                        bestX = cr.bestX;
                        bestY = cr.bestY;
                        foundSpot = true;
                    }
                }
            } else {
                for (const QPoint &pt : validCandidates) {
                    int testX = pt.x();
                    int testY = pt.y();
                    if (!checkCollision(grid, binW, binH, sprite, testX, testY)) {
                        slidePosition(grid, binW, binH, sprite, border, testX, testY);

                        long long score = evaluatePlacementScore(testX, testY, sprite.w, sprite.h, border,
                                                                 currentUsedW, currentUsedH,
                                                                 options.forceSquare);
                        if (bestScore < 0 || score < bestScore) {
                            bestScore = score;
                            bestX = testX;
                            bestY = testY;
                            foundSpot = true;
                        }
                    }
                }
            }

            // Fallback dense scan if no candidate was collision-free
            if (!foundSpot) {
                for (int y = border; y <= binH - sprite.h - border; y += 8) {
                    for (int x = border; x <= binW - sprite.w - border; x += 8) {
                        if (!checkCollision(grid, binW, binH, sprite, x, y)) {
                            int sx = x;
                            int sy = y;
                            slidePosition(grid, binW, binH, sprite, border, sx, sy);
                            long long score = evaluatePlacementScore(sx, sy, sprite.w, sprite.h, border,
                                                                     currentUsedW, currentUsedH,
                                                                     options.forceSquare);
                            if (bestScore < 0 || score < bestScore) {
                                bestScore = score;
                                bestX = sx;
                                bestY = sy;
                                foundSpot = true;
                            }
                        }
                    }
                    if (foundSpot && y > currentUsedH + 64) break;
                }
            }

            if (!foundSpot) {
                allPacked = false;
                break;
            }

            placements[sIdx] = QPoint(bestX, bestY);
            stampMask(grid, binW, sprite, bestX, bestY);

            placed.push_back({ bestX, bestY, sprite.w, sprite.h });
            currentUsedW = std::max(currentUsedW, bestX + sprite.w + border);
            currentUsedH = std::max(currentUsedH, bestY + sprite.h + border);
        }

        if (!allPacked) {
            // Expand bin
            if (options.powerOfTwo) {
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
            } else {
                int nextW = std::min(static_cast<int>(binW * 1.2) + 32, options.maxWidth + 1);
                int nextH = std::min(static_cast<int>(binH * 1.2) + 32, options.maxHeight + 1);
                if (nextW <= binW && nextH <= binH) {
                    break;
                }
                binW = nextW;
                binH = nextH;
            }
        }
    }

    if (!allPacked) {
        return result; // Could not fit within maxWidth / maxHeight
    }

    // 5. Determine actual used bounds
    int usedW = 0, usedH = 0;
    for (size_t i = 0; i < sprites.size(); ++i) {
        usedW = std::max(usedW, placements[i].x() + sprites[i].w + border);
        usedH = std::max(usedH, placements[i].y() + sprites[i].h + border);
    }

    int finalW = usedW;
    int finalH = usedH;
    if (options.powerOfTwo) {
        finalW = nextPowerOfTwo(usedW);
        finalH = nextPowerOfTwo(usedH);
        if (options.forceSquare) {
            finalW = std::max(finalW, finalH);
            finalH = finalW;
        }
    } else {
        finalW = usedW;
        finalH = usedH;
        if (options.forceSquare) {
            finalW = std::max(finalW, finalH);
            finalH = finalW;
        }
    }

    // 6. Generate final atlas image and populate frameRects
    QImage atlas(finalW, finalH, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::transparent);
    QPainter painter(&atlas);

    // Map unique placements back to original input order
    QVector<QRect> uniqueRects(uniqueFrames.size());
    for (size_t sIdx = 0; sIdx < sprites.size(); ++sIdx) {
        const SpriteToPack &s = sprites[sIdx];
        const QPoint &pos = placements[sIdx];
        painter.drawImage(pos, s.image);

        if (extrude > 0) {
            AtlasPacker::applyExtrusion(atlas, QRect(pos.x(), pos.y(), s.w, s.h), s.image, extrude);
        }
        uniqueRects[s.origIndex] = QRect(pos.x(), pos.y(), s.w, s.h);
    }
    painter.end();

    // Map according to original frames mapping
    result.frameRects.resize(mapping.size());
    for (int i = 0; i < mapping.size(); ++i) {
        int uIdx = mapping[i];
        if (uIdx >= 0 && uIdx < uniqueRects.size()) {
            result.frameRects[i] = uniqueRects[uIdx];
        } else if (i < uniqueRects.size()) {
            result.frameRects[i] = uniqueRects[i];
        }
    }

    result.atlas = atlas;
    result.dimensions = QSize(finalW, finalH);
    long long atlasArea = static_cast<long long>(finalW) * finalH;
    result.efficiency = (atlasArea > 0) ? std::min(100.0f, static_cast<float>(totalBoundingArea) / atlasArea * 100.0f) : 0.0f;
    result.success = true;

    return result;
}
