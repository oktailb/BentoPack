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

#include "geometry/polygonsimplifier.h"
#include <cmath>
#include <algorithm>

namespace SpriteStudioGeometry {

namespace {

double perpendicularDistance(const QPointF &pt, const QPointF &lineStart, const QPointF &lineEnd)
{
    double dx = lineEnd.x() - lineStart.x();
    double dy = lineEnd.y() - lineStart.y();
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-9) {
        double px = pt.x() - lineStart.x();
        double py = pt.y() - lineStart.y();
        return std::sqrt(px * px + py * py);
    }
    double num = std::abs(dy * pt.x() - dx * pt.y() + lineEnd.x() * lineStart.y() - lineEnd.y() * lineStart.x());
    return num / std::sqrt(lenSq);
}

void rdpRecursive(const QVector<QPointF> &pts, int start, int end, double eps, QVector<int> &keepIndices)
{
    if (end <= start + 1) {
        return;
    }

    double maxDist = 0.0;
    int indexFarthest = start;

    for (int i = start + 1; i < end; ++i) {
        double d = perpendicularDistance(pts[i], pts[start], pts[end]);
        if (d > maxDist) {
            maxDist = d;
            indexFarthest = i;
        }
    }

    if (maxDist > eps) {
        keepIndices.append(indexFarthest);
        rdpRecursive(pts, start, indexFarthest, eps, keepIndices);
        rdpRecursive(pts, indexFarthest, end, eps, keepIndices);
    }
}

// Ensure polygon winding is counter-clockwise (CCW)
QPolygonF ensureCounterClockwise(const QPolygonF &poly)
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

} // namespace

QPolygonF PolygonSimplifier::simplify(const QPolygonF &rawContour,
                                      double epsilon,
                                      double padding,
                                      int maxVertices,
                                      const QSize &bounds)
{
    if (rawContour.size() <= 3) {
        return rawContour;
    }

    QVector<QPointF> pts;
    for (const QPointF &p : rawContour) {
        if (pts.isEmpty() || pts.last() != p) {
            pts.append(p);
        }
    }
    if (pts.size() > 1 && pts.first() == pts.last()) {
        pts.removeLast();
    }

    int n = pts.size();
    if (n <= 3) {
        return QPolygonF(pts);
    }

    // Find the pair of points furthest apart (diameter) to split closed loop into 2 chains
    int idxA = 0;
    int idxB = n / 2;
    double maxDistSq = 0.0;

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dx = pts[i].x() - pts[j].x();
            double dy = pts[i].y() - pts[j].y();
            double d2 = dx * dx + dy * dy;
            if (d2 > maxDistSq) {
                maxDistSq = d2;
                idxA = i;
                idxB = j;
            }
        }
    }

    if (idxA > idxB) std::swap(idxA, idxB);

    // Build Chain 1 (A -> B) and Chain 2 (B -> A)
    QVector<QPointF> chain1;
    for (int i = idxA; i <= idxB; ++i) chain1.append(pts[i]);

    QVector<QPointF> chain2;
    for (int i = idxB; i < n; ++i) chain2.append(pts[i]);
    for (int i = 0; i <= idxA; ++i) chain2.append(pts[i]);

    double currentEps = std::max(0.2, epsilon);
    QPolygonF simplified;

    // Iterative RDP in case result exceeds maxVertices
    for (int iter = 0; iter < 10; ++iter) {
        QVector<int> keep1 = {0, static_cast<int>(chain1.size() - 1)};
        rdpRecursive(chain1, 0, chain1.size() - 1, currentEps, keep1);
        std::sort(keep1.begin(), keep1.end());

        QVector<int> keep2 = {0, static_cast<int>(chain2.size() - 1)};
        rdpRecursive(chain2, 0, chain2.size() - 1, currentEps, keep2);
        std::sort(keep2.begin(), keep2.end());

        simplified.clear();
        for (int idx : keep1) {
            simplified.append(chain1[idx]);
        }
        // Exclude first and last of chain2 since they duplicate B and A
        for (int i = 1; i < keep2.size() - 1; ++i) {
            simplified.append(chain2[keep2[i]]);
        }

        if (maxVertices <= 3 || simplified.size() <= maxVertices) {
            break;
        }
        currentEps *= 1.35; // Increase tolerance to reduce vertex count
    }

    if (simplified.size() < 3) {
        return QPolygonF(pts);
    }

    simplified = ensureCounterClockwise(simplified);

    // Apply outward normal dilation (padding)
    if (padding > 0.01) {
        int m = simplified.size();
        QPolygonF padded;
        for (int i = 0; i < m; ++i) {
            int prev = (i - 1 + m) % m;
            int next = (i + 1) % m;

            QPointF p0 = simplified[prev];
            QPointF p1 = simplified[i];
            QPointF p2 = simplified[next];

            QPointF e1 = p1 - p0;
            QPointF e2 = p2 - p1;

            double len1 = std::hypot(e1.x(), e1.y());
            double len2 = std::hypot(e2.x(), e2.y());

            if (len1 > 1e-6) e1 /= len1;
            if (len2 > 1e-6) e2 /= len2;

            // Outward normal in screen coordinates (CW winding)
            QPointF n1(e1.y(), -e1.x());
            QPointF n2(e2.y(), -e2.x());

            // 2D cross product e1 x e2 = sin(turn_angle)
            double z = e1.x() * e2.y() - e1.y() * e2.x();

            QPointF offsetVec;
            if (std::abs(z) < 1e-4) {
                // Collinear edges
                offsetVec = n1 * padding;
            } else {
                // Exact miter offset intersection: v = padding * (e1 - e2) / z
                // Projects to exactly 'padding' onto both outward edge normals n1 and n2
                offsetVec = padding * (e1 - e2) / z;

                // Clamp miter spike (max 2.5x padding) to avoid extreme needle-like protrusions
                double vlen = std::hypot(offsetVec.x(), offsetVec.y());
                double maxMiter = padding * 2.5;
                if (vlen > maxMiter && vlen > 1e-6) {
                    offsetVec = (offsetVec / vlen) * maxMiter;
                }
            }

            QPointF newPt = p1 + offsetVec;

            // Clamp to bounds if provided
            if (bounds.isValid() && bounds.width() > 0 && bounds.height() > 0) {
                newPt.setX(std::clamp(newPt.x(), 0.0, static_cast<double>(bounds.width())));
                newPt.setY(std::clamp(newPt.y(), 0.0, static_cast<double>(bounds.height())));
            }

            padded.append(newPt);
        }
        simplified = padded;
    }

    return simplified;
}

} // namespace SpriteStudioGeometry
