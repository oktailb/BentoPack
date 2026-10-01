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

#include "geometry/polygonsimplifier.h"
#include "geometry/contourtracer.h"
#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>

namespace BentoPackGeometry {

namespace {

// Dilates the alpha channel directly with a circular disk kernel of radius 'padding'.
// This computes the exact Minkowski sum, bridging narrow slots and preventing reflex miter inversions.
QImage dilateAlphaMask(const QImage &src, int alphaThreshold, double radius)
{
    int w = src.width();
    int h = src.height();
    QImage dilated(w, h, QImage::Format_ARGB32);
    dilated.fill(Qt::transparent);

    if (radius < 0.5) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (qAlpha(src.pixel(x, y)) >= alphaThreshold) {
                    dilated.setPixel(x, y, qRgba(255, 255, 255, 255));
                }
            }
        }
        return dilated;
    }

    int r = static_cast<int>(std::ceil(radius));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (qAlpha(src.pixel(x, y)) >= alphaThreshold) {
                for (int dy = -r; dy <= r; ++dy) {
                    int ny = y + dy;
                    if (ny < 0 || ny >= h) continue;
                    for (int dx = -r; dx <= r; ++dx) {
                        int nx = x + dx;
                        if (nx < 0 || nx >= w) continue;
                        if (dx * dx + dy * dy <= radius * radius + 0.25) {
                            dilated.setPixel(nx, ny, qRgba(255, 255, 255, 255));
                        }
                    }
                }
            }
        }
    }
    return dilated;
}

// Area of triangle formed by 3 vertices (Visvalingam-Whyatt geometric importance)
double triangleArea(const QPointF &a, const QPointF &b, const QPointF &c)
{
    return 0.5 * std::abs((b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y()));
}

// Checks whether segment (p1, p2) traverses any pixel with alpha >= alphaThreshold in original sprite
bool segmentCrossesOpaque(const QPointF &p1, const QPointF &p2, const QImage &img, int alphaThreshold)
{
    if (img.isNull() || img.width() <= 0 || img.height() <= 0) return false;

    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double dist = std::hypot(dx, dy);
    if (dist < 0.75) return false;

    int w = img.width();
    int h = img.height();

    // If both endpoints are on the exact same outer canvas boundary, the segment lies along the boundary
    if (std::abs(p1.x()) < 1e-4 && std::abs(p2.x()) < 1e-4) return false;
    if (std::abs(p1.y()) < 1e-4 && std::abs(p2.y()) < 1e-4) return false;
    if (std::abs(p1.x() - w) < 1e-4 && std::abs(p2.x() - w) < 1e-4) return false;
    if (std::abs(p1.y() - h) < 1e-4 && std::abs(p2.y() - h) < 1e-4) return false;

    // Step every 0.5 pixels along segment
    int steps = std::max(2, static_cast<int>(std::ceil(dist * 2.0)));

    for (int s = 1; s < steps; ++s) {
        double t = static_cast<double>(s) / steps;
        double px = p1.x() + t * dx;
        double py = p1.y() + t * dy;

        // Points on or outside the canvas perimeter are on the outer envelope, not piercing the sprite interior
        if (px <= 0.0 || px >= w || py <= 0.0 || py >= h) continue;

        int x = std::floor(px);
        int y = std::floor(py);
        if (x >= 0 && x < w && y >= 0 && y < h) {
            if (qAlpha(img.pixel(x, y)) >= alphaThreshold) {
                // If point lies on the top border of pixel row y and the pixel above is transparent,
                // it is tracing the outer boundary, not penetrating the opaque interior.
                if (std::abs(py - y) < 1e-4 && y > 0 && qAlpha(img.pixel(x, y - 1)) < alphaThreshold) {
                    continue;
                }
                // If point lies on the left border of pixel col x and the pixel to the left is transparent,
                // it is tracing the outer boundary, not penetrating the opaque interior.
                if (std::abs(px - x) < 1e-4 && x > 0 && qAlpha(img.pixel(x - 1, y)) < alphaThreshold) {
                    continue;
                }
                return true;
            }
        }
    }
    return false;
}

bool segmentsIntersect(const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4)
{
    auto ccw = [](const QPointF &a, const QPointF &b, const QPointF &c) {
        return (c.y() - a.y()) * (b.x() - a.x()) > (b.y() - a.y()) * (c.x() - a.x());
    };
    return (ccw(p1, p3, p4) != ccw(p2, p3, p4)) && (ccw(p1, p2, p3) != ccw(p1, p2, p4));
}

// Ensures polygon winding is clockwise on screen (positive signed area with Y down)
QPolygonF ensureScreenClockwise(const QPolygonF &poly)
{
    if (poly.size() < 3) return poly;
    double signedArea = 0.0;
    int n = poly.size();
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        signedArea += poly[i].x() * poly[next].y() - poly[next].x() * poly[i].y();
    }
    if (signedArea < 0.0) {
        QPolygonF rev;
        for (int i = n - 1; i >= 0; --i) {
            rev.append(poly[i]);
        }
        return rev;
    }
    return poly;
}

// Fallback robust outward bisector dilation when no source image is provided
QPolygonF dilateRobust(const QPolygonF &poly, double padding, const QSize &bounds)
{
    if (poly.size() < 3 || padding <= 0.01) return poly;
    int m = poly.size();
    QPolygonF padded;
    for (int i = 0; i < m; ++i) {
        int prev = (i - 1 + m) % m;
        int next = (i + 1) % m;

        QPointF p0 = poly[prev];
        QPointF p1 = poly[i];
        QPointF p2 = poly[next];

        QPointF e1 = p1 - p0;
        QPointF e2 = p2 - p1;

        double len1 = std::hypot(e1.x(), e1.y());
        double len2 = std::hypot(e2.x(), e2.y());

        if (len1 > 1e-6) e1 /= len1;
        if (len2 > 1e-6) e2 /= len2;

        // Outward normals for screen clockwise polygon (Y down)
        QPointF n1(e1.y(), -e1.x());
        QPointF n2(e2.y(), -e2.x());

        // Outward bisector direction
        QPointF b = n1 + n2;
        double blen = std::hypot(b.x(), b.y());
        QPointF offsetVec;
        if (blen < 1e-4) {
            offsetVec = n1 * padding;
        } else {
            b /= blen;
            double cosHalf = b.x() * n1.x() + b.y() * n1.y();
            if (cosHalf < 0.2) cosHalf = 0.2;
            double dist = std::min(padding * 2.5, padding / cosHalf);
            offsetVec = b * dist;
        }

        QPointF newPt = p1 + offsetVec;
        if (bounds.isValid() && bounds.width() > 0 && bounds.height() > 0) {
            newPt.setX(std::clamp(newPt.x(), 0.0, static_cast<double>(bounds.width())));
            newPt.setY(std::clamp(newPt.y(), 0.0, static_cast<double>(bounds.height())));
        }
        padded.append(newPt);
    }
    return padded;
}

} // namespace

QPolygonF PolygonSimplifier::simplify(const QPolygonF &rawContour,
                                      double epsilon,
                                      double padding,
                                      int maxVertices,
                                      const QSize &bounds,
                                      const QImage &image,
                                      int alphaThreshold)
{
    if (rawContour.size() <= 3) {
        return rawContour;
    }

    std::vector<QPointF> poly;

    // 1. If an image is provided and padding > 0.01, perform morphological dilation
    // of the alpha mask to guarantee a smooth, fillet-rounded expansion without reflex inversions.
    if (!image.isNull() && padding > 0.01) {
        QImage dilatedMask = dilateAlphaMask(image, alphaThreshold, padding);
        QPolygonF dilatedContour = BentoPackGeometry::ContourTracer::traceContour(dilatedMask, 128);
        if (!dilatedContour.isEmpty()) {
            QPolygonF oriented = ensureScreenClockwise(dilatedContour);
            for (const QPointF &p : oriented) {
                if (poly.empty() || poly.back() != p) poly.push_back(p);
            }
            if (poly.size() > 1 && poly.front() == poly.back()) poly.pop_back();
        }
    }

    // Fallback: If no image or dilation produced empty contour, use robust vector bisector dilation
    if (poly.empty()) {
        QPolygonF oriented = ensureScreenClockwise(rawContour);
        if (padding > 0.01) {
            oriented = dilateRobust(oriented, padding, bounds);
        }
        for (const QPointF &p : oriented) {
            if (poly.empty() || poly.back() != p) poly.push_back(p);
        }
        if (poly.size() > 1 && poly.front() == poly.back()) poly.pop_back();
    }

    if (poly.size() <= 3) {
        QPolygonF r;
        for (const auto &p : poly) r.append(p);
        return r;
    }

    // 2. Collinear and micro-step pre-filtering (removes 0-area points)
    bool changed = true;
    while (changed && poly.size() > 3) {
        changed = false;
        int sz = poly.size();
        for (int i = 0; i < sz; ++i) {
            int prev = (i - 1 + sz) % sz;
            int next = (i + 1) % sz;
            if (triangleArea(poly[prev], poly[i], poly[next]) < 1e-4) {
                if (!image.isNull() && segmentCrossesOpaque(poly[prev], poly[next], image, alphaThreshold)) {
                    continue;
                }
                poly.erase(poly.begin() + i);
                changed = true;
                break;
            }
        }
    }

    // 3. Pure Visvalingam-Whyatt Area Decimation:
    // Directly optimizes overdraw by eliminating vertices that have minimal triangle area impact,
    // prioritizing vertex allocation to large silhouette curves over microscopic pixel art staircases.
    double areaEps = epsilon * epsilon * 0.5;

    while (poly.size() > 3) {
        int sz = poly.size();
        int bestIdx = -1;
        double minArea = std::numeric_limits<double>::max();

        for (int i = 0; i < sz; ++i) {
            int prev = (i - 1 + sz) % sz;
            int next = (i + 1) % sz;

            // Invariant 1: New segment must NEVER cross non-transparent pixels in original image!
            if (!image.isNull() && segmentCrossesOpaque(poly[prev], poly[next], image, alphaThreshold)) {
                continue;
            }

            // Invariant 2: New segment must not cause polygon self-intersection
            bool self = false;
            for (int j = 0; j < sz; ++j) {
                int jnext = (j + 1) % sz;
                if (j == prev || j == i || jnext == prev || jnext == i || j == next || jnext == next) continue;
                if (segmentsIntersect(poly[prev], poly[next], poly[j], poly[jnext])) {
                    self = true;
                    break;
                }
            }
            if (self) continue;

            double a = triangleArea(poly[prev], poly[i], poly[next]);
            if (a < minArea) {
                minArea = a;
                bestIdx = i;
            }
        }

        if (bestIdx == -1) {
            // Cannot remove any more vertices without cutting opaque pixels or self-intersecting
            break;
        }

        // Stop if within vertex budget AND candidate triangle area exceeds area threshold
        if (static_cast<int>(poly.size()) <= maxVertices && minArea > areaEps) {
            break;
        }

        poly.erase(poly.begin() + bestIdx);
    }

    QPolygonF result;
    for (const auto &p : poly) result.append(p);
    return result;
}

} // namespace BentoPackGeometry
