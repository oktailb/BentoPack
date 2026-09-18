#include "geometry/contourtracer.h"
#include <QVector>
#include <QPointF>
#include <QMultiHash>
#include <cmath>
#include <algorithm>

namespace SpriteStudioGeometry {

namespace {

// Helper point key for fast segment chaining
struct GridPoint {
    int x2; // Coordinates scaled by 2 to keep integers (e.g. 0.5 -> 1)
    int y2;

    bool operator==(const GridPoint &other) const {
        return x2 == other.x2 && y2 == other.y2;
    }
};

inline size_t qHash(const GridPoint &p, size_t seed = 0) {
    return ::qHash(p.x2, seed) ^ (::qHash(p.y2, seed) << 16);
}

struct Segment {
    GridPoint a;
    GridPoint b;
    bool used = false;
};

double calculatePolygonArea(const QPolygonF &poly) {
    if (poly.size() < 3) return 0.0;
    double area = 0.0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        area += poly[i].x() * poly[next].y() - poly[next].x() * poly[i].y();
    }
    return std::abs(area) * 0.5;
}

} // namespace

QPolygonF ContourTracer::traceContour(const QImage &image, int alphaThreshold)
{
    QList<QPolygonF> all = traceAllContours(image, alphaThreshold);
    if (all.isEmpty()) {
        return QPolygonF();
    }

    // Return the contour enclosing the maximum area (primary outer boundary)
    int bestIdx = 0;
    double maxArea = 0.0;
    for (int i = 0; i < all.size(); ++i) {
        double a = calculatePolygonArea(all[i]);
        if (a > maxArea) {
            maxArea = a;
            bestIdx = i;
        }
    }

    return all[bestIdx];
}

QList<QPolygonF> ContourTracer::traceAllContours(const QImage &image, int alphaThreshold)
{
    QList<QPolygonF> result;
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return result;
    }

    QImage img = image.convertToFormat(QImage::Format_ARGB32);
    int w = img.width();
    int h = img.height();

    // Create 1-pixel padded binary grid: 1 if alpha >= threshold, 0 otherwise
    int gridW = w + 2;
    int gridH = h + 2;
    QVector<uint8_t> grid(gridW * gridH, 0);

    bool hasOpaque = false;
    for (int y = 0; y < h; ++y) {
        const QRgb *scan = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        int rowOffset = (y + 1) * gridW;
        for (int x = 0; x < w; ++x) {
            if (qAlpha(scan[x]) >= alphaThreshold) {
                grid[rowOffset + (x + 1)] = 1;
                hasOpaque = true;
            }
        }
    }

    if (!hasOpaque) {
        return result;
    }

    // Marching Squares edge generation
    // Each cell has 4 corners: TL, TR, BR, BL
    // Edge midpoints scaled by 2:
    // Top: (2x+1, 2y)
    // Right: (2x+2, 2y+1)
    // Bottom: (2x+1, 2y+2)
    // Left: (2x, 2y+1)
    QVector<Segment> segments;

    for (int y = 0; y < gridH - 1; ++y) {
        int r0 = y * gridW;
        int r1 = (y + 1) * gridW;
        for (int x = 0; x < gridW - 1; ++x) {
            uint8_t tl = grid[r0 + x];
            uint8_t tr = grid[r0 + x + 1];
            uint8_t br = grid[r1 + x + 1];
            uint8_t bl = grid[r1 + x];

            int caseIdx = (tl << 3) | (tr << 2) | (br << 1) | bl;
            if (caseIdx == 0 || caseIdx == 15) continue;

            GridPoint top{2 * x + 1, 2 * y};
            GridPoint right{2 * x + 2, 2 * y + 1};
            GridPoint bottom{2 * x + 1, 2 * y + 2};
            GridPoint left{2 * x, 2 * y + 1};

            switch (caseIdx) {
            case 1:  // BL
                segments.append({bottom, left});
                break;
            case 2:  // BR
                segments.append({right, bottom});
                break;
            case 3:  // BL + BR
                segments.append({right, left});
                break;
            case 4:  // TR
                segments.append({top, right});
                break;
            case 5:  // TR + BL (ambiguous, resolve connected)
                segments.append({top, right});
                segments.append({bottom, left});
                break;
            case 6:  // TR + BR
                segments.append({top, bottom});
                break;
            case 7:  // TR + BR + BL
                segments.append({top, left});
                break;
            case 8:  // TL
                segments.append({left, top});
                break;
            case 9:  // TL + BL
                segments.append({bottom, top});
                break;
            case 10: // TL + BR (ambiguous, resolve connected)
                segments.append({left, top});
                segments.append({right, bottom});
                break;
            case 11: // TL + BR + BL
                segments.append({right, top});
                break;
            case 12: // TL + TR
                segments.append({left, right});
                break;
            case 13: // TL + TR + BL
                segments.append({bottom, right});
                break;
            case 14: // TL + TR + BR
                segments.append({left, bottom});
                break;
            default:
                break;
            }
        }
    }

    if (segments.isEmpty()) {
        return result;
    }

    // Build lookup map from start GridPoint to segment index for fast chaining
    QMultiHash<GridPoint, int> startMap;
    for (int i = 0; i < segments.size(); ++i) {
        startMap.insert(segments[i].a, i);
    }

    // Chain segments into closed loops
    for (int i = 0; i < segments.size(); ++i) {
        if (segments[i].used) continue;

        QPolygonF loop;
        int currIdx = i;
        GridPoint startPt = segments[currIdx].a;
        GridPoint currPt = startPt;

        while (currIdx >= 0 && !segments[currIdx].used) {
            segments[currIdx].used = true;
            // Convert grid coords back to local image pixel coordinates
            // Grid point (gx, gy) is at pixel center (gx - 0.5, gy - 0.5)
            // Midpoints between grid cells align with outer pixel boundaries at (x2 / 2.0) - 0.5
            double px = (static_cast<double>(segments[currIdx].a.x2) / 2.0) - 0.5;
            double py = (static_cast<double>(segments[currIdx].a.y2) / 2.0) - 0.5;
            loop.append(QPointF(px, py));

            currPt = segments[currIdx].b;
            if (currPt == startPt) {
                // Loop closed!
                break;
            }

            // Find next connecting segment starting at currPt
            int nextIdx = -1;
            auto it = startMap.find(currPt);
            while (it != startMap.end() && it.key() == currPt) {
                int cand = it.value();
                if (!segments[cand].used) {
                    nextIdx = cand;
                    break;
                }
                ++it;
            }
            currIdx = nextIdx;
        }

        if (loop.size() >= 3) {
            result.append(loop);
        }
    }

    return result;
}

} // namespace SpriteStudioGeometry
