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

    // Fast-path: If image has no alpha channel or is completely opaque (e.g. photos/wallpapers),
    // it is guaranteed to be a single image component rather than segmented sprites.
    if (!sourceImage.hasAlphaChannel()) {
        SpriteBox singleBox;
        singleBox.rect = QRect(0, 0, w, h);
        outBoxes.append(singleBox);
        outFrames.append(image);
        if (progressCallback) progressCallback(100);
        return true;
    }

    std::vector<const QRgb*> scanLines(h);
    for (int y = 0; y < h; ++y) {
        scanLines[y] = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    }

    // Quick check if the image has any transparent pixels below alpha threshold
    bool hasAnyTransparentPixel = false;
    for (int y = 0; y < h; ++y) {
        const QRgb *lineY = scanLines[y];
        for (int x = 0; x < w; ++x) {
            if (qAlpha(lineY[x]) <= ALPHA_THRESHOLD) {
                hasAnyTransparentPixel = true;
                break;
            }
        }
        if (hasAnyTransparentPixel) break;
    }

    if (!hasAnyTransparentPixel) {
        SpriteBox singleBox;
        singleBox.rect = QRect(0, 0, w, h);
        outBoxes.append(singleBox);
        outFrames.append(image);
        if (progressCallback) progressCallback(100);
        return true;
    }

    std::vector<int> componentIdAtPixel(static_cast<size_t>(w * h), -1);
    QList<QRect> componentRects;
    std::vector<Point2D> stack;
    stack.reserve(4096);

    for (int y = 0; y < h; ++y) {
        if (progressCallback && (y % 100 == 0 || y == h - 1)) {
            progressCallback(static_cast<int>((static_cast<double>(y) / h) * 50.0));
        }
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

    // Collect master component rectangles
    QList<QRect> initialRects;
    for (int c = 0; c < componentRects.size(); ++c) {
        if (isMaster[c]) initialRects.append(componentRects[c]);
    }

    // Advanced constraints: Max dimensions, permissive dimension, grid alignment, non-overlapping boxes
    QList<QRect> finalRects;
    const int maxW = options.maxWidth > 0 ? options.maxWidth : 0;
    const int maxH = options.maxHeight > 0 ? options.maxHeight : 0;
    const int perm = options.maxDimensionPermissive > 0 ? options.maxDimensionPermissive : 0;
    const int minSz = std::max(1, options.minSliceSize);

    // 1. Partition oversized components if max dimensions are defined
    for (const QRect &r : initialRects) {
        bool withinLimits = true;
        if (maxW > 0 && maxH > 0) {
            bool normalFit = (r.width() <= maxW && r.height() <= maxH);
            bool permissiveFit = false;
            if (perm > 0) {
                if (r.width() <= maxW && r.height() <= perm) permissiveFit = true;
                if (r.height() <= maxH && r.width() <= perm) permissiveFit = true;
            }
            withinLimits = (normalFit || permissiveFit);
        }

        if (withinLimits) {
            finalRects.append(r);
        } else {
            // Partition oversized block into tiles of size maxW x maxH
            int stepX = maxW > 0 ? maxW : r.width();
            int stepY = maxH > 0 ? maxH : r.height();
            if (options.alignToGrid && options.gridCellSize > 0) {
                stepX = std::max(options.gridCellSize, (stepX / options.gridCellSize) * options.gridCellSize);
                stepY = std::max(options.gridCellSize, (stepY / options.gridCellSize) * options.gridCellSize);
            }

            for (int sy = r.top(); sy <= r.bottom(); sy += stepY) {
                int cellH = std::min(stepY, r.bottom() - sy + 1);
                for (int sx = r.left(); sx <= r.right(); sx += stepX) {
                    int cellW = std::min(stepX, r.right() - sx + 1);

                    // Compute tight bounding box of pixels within this cell
                    int subMinX = sx + cellW, subMinY = sy + cellH, subMaxX = -1, subMaxY = -1;
                    for (int py = sy; py < sy + cellH; ++py) {
                        const QRgb *line = scanLines[py];
                        for (int px = sx; px < sx + cellW; ++px) {
                            if (qAlpha(line[px]) > ALPHA_THRESHOLD) {
                                if (px < subMinX) subMinX = px;
                                if (py < subMinY) subMinY = py;
                                if (px > subMaxX) subMaxX = px;
                                if (py > subMaxY) subMaxY = py;
                            }
                        }
                    }

                    if (subMinX <= subMaxX && subMinY <= subMaxY) {
                        int bw = subMaxX - subMinX + 1;
                        int bh = subMaxY - subMinY + 1;
                        if (bw >= minSz && bh >= minSz) {
                            finalRects.append(QRect(subMinX, subMinY, bw, bh));
                        }
                    }
                }
            }
        }
    }

    // 2. Align to grid if requested
    if (options.alignToGrid && options.gridCellSize > 1) {
        const int g = options.gridCellSize;
        for (int i = 0; i < finalRects.size(); ++i) {
            QRect &r = finalRects[i];
            int left = (r.left() / g) * g;
            int top = (r.top() / g) * g;
            int right = std::min(w - 1, ((r.right() + g) / g) * g - 1);
            int bottom = std::min(h - 1, ((r.bottom() + g) / g) * g - 1);
            r = QRect(left, top, right - left + 1, bottom - top + 1);
        }
    }

    // 3. Resolve overlaps (disallowOverlaps)
    if (options.disallowOverlaps && finalRects.size() > 1) {
        bool changed = true;
        int passCount = 0;
        while (changed && passCount < 30) {
            changed = false;
            passCount++;

            for (int i = 0; i < finalRects.size(); ++i) {
                for (int j = i + 1; j < finalRects.size(); ++j) {
                    QRect &ri = finalRects[i];
                    QRect &rj = finalRects[j];
                    if (!ri.intersects(rj)) continue;

                    // Option A: If merging them stays within max limits, merge!
                    QRect united = ri.united(rj);
                    bool canMerge = true;
                    if (maxW > 0 && maxH > 0) {
                        bool normFit = (united.width() <= maxW && united.height() <= maxH);
                        bool permFit = false;
                        if (perm > 0) {
                            if (united.width() <= maxW && united.height() <= perm) permFit = true;
                            if (united.height() <= maxH && united.width() <= perm) permFit = true;
                        }
                        canMerge = (normFit || permFit);
                    }

                    if (canMerge) {
                        ri = united;
                        finalRects.removeAt(j);
                        changed = true;
                        break;
                    }

                    // Option B: Cannot merge without exceeding max sprite size.
                    // Split the overlap along the axis of least penetration to disallow crossing bounding boxes.
                    QRect inter = ri.intersected(rj);
                    if (inter.width() < inter.height()) {
                        // Horizontal resolution
                        int midX = inter.left() + inter.width() / 2;
                        if (ri.center().x() < rj.center().x()) {
                            ri.setRight(midX - 1);
                            rj.setLeft(midX);
                        } else {
                            rj.setRight(midX - 1);
                            ri.setLeft(midX);
                        }
                    } else {
                        // Vertical resolution
                        int midY = inter.top() + inter.height() / 2;
                        if (ri.center().y() < rj.center().y()) {
                            ri.setBottom(midY - 1);
                            rj.setTop(midY);
                        } else {
                            rj.setBottom(midY - 1);
                            ri.setTop(midY);
                        }
                    }
                    changed = true;
                    break;
                }
                if (changed) break;
            }
        }

        // Clean up degenerate rectangles after clipping
        QList<QRect> validClean;
        for (const QRect &r : finalRects) {
            if (r.width() >= minSz && r.height() >= minSz && r.right() < w && r.bottom() < h) {
                validClean.append(r);
            }
        }
        finalRects = validClean;
    }

    // Sort final components in visual reading order
    std::sort(finalRects.begin(), finalRects.end(),
              [verticalTolerance](const QRect &ra, const QRect &rb) {
                  if (qAbs(ra.y() - rb.y()) <= verticalTolerance)
                      return ra.x() < rb.x();
                  return ra.y() < rb.y();
              });

    // Build bounding boxes & extract frames
    outBoxes.reserve(finalRects.size());
    outFrames.reserve(finalRects.size());

    for (int i = 0; i < finalRects.size(); ++i) {
        SpriteBox box;
        box.rect = finalRects[i];
        box.index = i;
        box.selected = false;
        box.groupId = -1;
        outBoxes.append(box);

        QImage frame = image.copy(box.rect);
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
