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

#include "image/spritedetector.h"
#include "config/appconfig.h"
#include <vector>
#include <algorithm>

namespace {
struct Point2D {
    int x;
    int y;
};

/**
 * @brief 2D uniform spatial grid for accelerating rectangle containment queries.
 * Reduces all-pairs N x N containment checks from O(N^2) to O(N) average time.
 */
class SpatialGrid2D {
public:
    SpatialGrid2D(int imageWidth, int imageHeight, int numRectangles)
    {
        m_width = std::max(1, imageWidth);
        m_height = std::max(1, imageHeight);

        // Adapt cell size based on image dimensions and rectangle density:
        // Targeting ~16 to ~64 cells along the longest side
        const int maxDim = std::max(m_width, m_height);
        int targetCellsPerSide = 32;
        if (numRectangles > 2000) {
            targetCellsPerSide = 64;
        } else if (numRectangles < 100) {
            targetCellsPerSide = 16;
        }
        m_cellSize = std::clamp(maxDim / targetCellsPerSide, 16, 128);

        m_cols = std::max(1, (m_width + m_cellSize - 1) / m_cellSize);
        m_rows = std::max(1, (m_height + m_cellSize - 1) / m_cellSize);

        m_cells.resize(static_cast<size_t>(m_cols * m_rows));
    }

    void insert(int rectIndex, const QRect &rect)
    {
        const int minCol = std::clamp(rect.left() / m_cellSize, 0, m_cols - 1);
        const int maxCol = std::clamp(rect.right() / m_cellSize, 0, m_cols - 1);
        const int minRow = std::clamp(rect.top() / m_cellSize, 0, m_rows - 1);
        const int maxRow = std::clamp(rect.bottom() / m_cellSize, 0, m_rows - 1);

        for (int r = minRow; r <= maxRow; ++r) {
            const int rowOffset = r * m_cols;
            for (int c = minCol; c <= maxCol; ++c) {
                m_cells[rowOffset + c].push_back(rectIndex);
            }
        }
    }

    const std::vector<int>& getCandidatesAtPoint(int x, int y) const
    {
        const int c = std::clamp(x / m_cellSize, 0, m_cols - 1);
        const int r = std::clamp(y / m_cellSize, 0, m_rows - 1);
        return m_cells[r * m_cols + c];
    }

private:
    int m_width;
    int m_height;
    int m_cellSize;
    int m_cols;
    int m_rows;
    std::vector<std::vector<int>> m_cells;
};
}

bool SpriteDetector::detectToImages(const QImage &sourceImage,
                                    QList<QImage> &outFrames,
                                    QList<SpriteBox> &outBoxes,
                                    const SpriteDetectionOptions &options,
                                    std::function<void(int)> progressCallback)
{
    outFrames.clear();
    outBoxes.clear();

    if (sourceImage.isNull()) {
        return false;
    }

    QImage image = (sourceImage.format() == QImage::Format_ARGB32)
        ? sourceImage
        : sourceImage.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0) return true;

    const int ALPHA_THRESHOLD = (options.alphaThreshold < 0)
        ? AppConfig::instance().atlas().defaultAlphaThreshold
        : options.alphaThreshold;
    const int verticalTolerance = (options.verticalTolerance < 0)
        ? AppConfig::instance().atlas().defaultVerticalTolerance
        : options.verticalTolerance;

    std::vector<int> componentIdAtPixel(static_cast<size_t>(w * h), -1);
    QList<QRect> componentRects;
    std::vector<Point2D> stack;
    stack.reserve(4096);

    std::vector<const QRgb*> scanLines(h);
    for (int y = 0; y < h; ++y) {
        scanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    for (int y = 0; y < h; ++y) {
        const QRgb *lineY = scanLines[y];
        const int y_w = y * w;
        for (int x = 0; x < w; ++x) {
            const int idx = y_w + x;
            if (componentIdAtPixel[idx] >= 0 || qAlpha(lineY[x]) <= ALPHA_THRESHOLD)
                continue;

            const int componentId = static_cast<int>(componentRects.size());
            int minX = w, minY = h, maxX = -1, maxY = -1;

            stack.clear();
            stack.push_back({x, y});
            componentIdAtPixel[idx] = componentId;

            while (!stack.empty()) {
                Point2D p = stack.back();
                stack.pop_back();
                int cx = p.x, cy = p.y;
                if (cx < minX) minX = cx;
                if (cy < minY) minY = cy;
                if (cx > maxX) maxX = cx;
                if (cy > maxY) maxY = cy;

                // Up
                if (cy > 0) {
                    int ny = cy - 1, nx = cx;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Down
                if (cy + 1 < h) {
                    int ny = cy + 1, nx = cx;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Left
                if (cx > 0) {
                    int ny = cy, nx = cx - 1;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
                // Right
                if (cx + 1 < w) {
                    int ny = cy, nx = cx + 1;
                    int nidx = ny * w + nx;
                    if (componentIdAtPixel[nidx] < 0 && qAlpha(scanLines[ny][nx]) > ALPHA_THRESHOLD) {
                        componentIdAtPixel[nidx] = componentId;
                        stack.push_back({nx, ny});
                    }
                }
            }

            if (minX <= maxX && minY <= maxY) {
                componentRects.append(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1));
            }
        }
    }

    if (componentRects.isEmpty()) {
        if (progressCallback) progressCallback(100);
        return true;
    }

    if (progressCallback) progressCallback(50);

    // Filter components fully contained inside another using spatial grid partitioning O(N)
    const int numComponents = componentRects.size();
    QList<bool> isMaster(numComponents, true);

    if (numComponents > 1) {
        SpatialGrid2D grid(w, h, numComponents);
        for (int i = 0; i < numComponents; ++i) {
            grid.insert(i, componentRects[i]);
        }

        for (int i = 0; i < numComponents; ++i) {
            const QRect &ri = componentRects[i];
            const int riLeft = ri.left();
            const int riRight = ri.right();
            const int riTop = ri.top();
            const int riBottom = ri.bottom();
            const int riWidth = ri.width();
            const int riHeight = ri.height();

            // Any rectangle containing ri must contain the point (riLeft, riTop),
            // and therefore must be present in the spatial grid cell covering that point.
            const std::vector<int> &candidates = grid.getCandidatesAtPoint(riLeft, riTop);
            for (int j : candidates) {
                if (i == j) continue;

                const QRect &rj = componentRects[j];
                // Quick rejection: a containing rectangle must be at least as wide and tall
                if (rj.width() < riWidth || rj.height() < riHeight) continue;

                // Full geometric containment check
                if (rj.left() <= riLeft && rj.right() >= riRight &&
                    rj.top() <= riTop && rj.bottom() >= riBottom) {
                    // Tie-breaker: if two components have the exact same bounding box,
                    // keep the one with the smaller index to avoid discarding both
                    if (rj.left() == riLeft && rj.right() == riRight &&
                        rj.top() == riTop && rj.bottom() == riBottom && j > i) {
                        continue;
                    }
                    isMaster[i] = false;
                    break;
                }
            }
        }
    }

    // Sort master components in reading order
    QList<int> masterComponentIds;
    for (int c = 0; c < componentRects.size(); ++c) {
        if (isMaster[c]) masterComponentIds.append(c);
    }

    std::sort(masterComponentIds.begin(), masterComponentIds.end(),
              [&componentRects, verticalTolerance](int a, int b) {
                  const QRect &ra = componentRects[a], &rb = componentRects[b];
                  if (qAbs(ra.y() - rb.y()) <= verticalTolerance)
                      return ra.x() < rb.x();
                  return ra.y() < rb.y();
              });

    // Map component -> slice index
    std::vector<int> componentIdToBoxIndex(componentRects.size(), -1);
    for (int i = 0; i < masterComponentIds.size(); ++i) {
        componentIdToBoxIndex[masterComponentIds[i]] = i;
    }

    // Build pixel ownership
    std::vector<int> pixelOwner(static_cast<size_t>(w * h), -1);
    for (int idx = 0; idx < w * h; ++idx) {
        int c = componentIdAtPixel[idx];
        if (c >= 0) {
            pixelOwner[idx] = componentIdToBoxIndex[c];
        }
    }

    // Build bounding boxes & extract frames via direct scanline memory
    outBoxes.reserve(masterComponentIds.size());
    outFrames.reserve(masterComponentIds.size());

    for (int i = 0; i < masterComponentIds.size(); ++i) {
        SpriteBox box;
        box.rect = componentRects[masterComponentIds[i]];
        box.index = i;
        box.selected = false;
        box.groupId = -1;
        outBoxes.append(box);

        QImage frame = image.copy(box.rect);
        const int fw = frame.width();
        const int fh = frame.height();
        const int bx = box.rect.x();
        const int by = box.rect.y();

        for (int ly = 0; ly < fh; ++ly) {
            QRgb *frameLine = reinterpret_cast<QRgb*>(frame.scanLine(ly));
            const int gy_w = (by + ly) * w;
            for (int lx = 0; lx < fw; ++lx) {
                if (pixelOwner[gy_w + (bx + lx)] != i) {
                    frameLine[lx] = 0; // transparent
                }
            }
        }
        outFrames.append(frame);
    }

    if (progressCallback) progressCallback(100);
    return true;
}

bool SpriteDetector::detectBoxes(const QImage &sourceImage,
                                 QList<SpriteBox> &outBoxes,
                                 const SpriteDetectionOptions &options,
                                 std::function<void(int)> progressCallback)
{
    QList<QImage> unusedFrames;
    return detectToImages(sourceImage, unusedFrames, outBoxes, options, progressCallback);
}
