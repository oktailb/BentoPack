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

#include "geometry/triangulator.h"
#include <QVector>
#include <cmath>
#include <algorithm>

namespace BentoPackGeometry {

namespace {

inline double crossProduct(const QPointF &a, const QPointF &b, const QPointF &c)
{
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool isPointInTriangle(const QPointF &pt, const QPointF &a, const QPointF &b, const QPointF &c)
{
    double cp1 = crossProduct(a, b, pt);
    double cp2 = crossProduct(b, c, pt);
    double cp3 = crossProduct(c, a, pt);

    bool hasNeg = (cp1 < -1e-7) || (cp2 < -1e-7) || (cp3 < -1e-7);
    bool hasPos = (cp1 > 1e-7) || (cp2 > 1e-7) || (cp3 > 1e-7);

    return !(hasNeg && hasPos);
}

bool isEar(int u, int v, int w, const QVector<int> &indices, const QPolygonF &pts)
{
    const QPointF &a = pts[u];
    const QPointF &b = pts[v];
    const QPointF &c = pts[w];

    // Check if angle at B is convex (crossProduct > 0 in CCW)
    if (crossProduct(a, b, c) <= 1e-9) {
        return false;
    }

    // Check if any other polygon vertex is inside triangle (a, b, c)
    for (int idx : indices) {
        if (idx == u || idx == v || idx == w) continue;
        if (isPointInTriangle(pts[idx], a, b, c)) {
            return false;
        }
    }

    return true;
}

} // namespace

QList<int> Triangulator::triangulate(const QPolygonF &polygon)
{
    QList<int> triangles;
    int n = polygon.size();
    if (n < 3) {
        return triangles;
    }

    // Copy and remove duplicate closing point if present
    QPolygonF pts = polygon;
    if (pts.size() > 1 && pts.first() == pts.last()) {
        pts.removeLast();
    }
    n = pts.size();
    if (n < 3) {
        return triangles;
    }

    // Ensure counter-clockwise (CCW) winding
    double signedArea = 0.0;
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        signedArea += pts[i].x() * pts[next].y() - pts[next].x() * pts[i].y();
    }

    QVector<int> indices(n);
    if (signedArea < 0.0) {
        // Reverse indices for CCW
        for (int i = 0; i < n; ++i) indices[i] = n - 1 - i;
    } else {
        for (int i = 0; i < n; ++i) indices[i] = i;
    }

    int count = n;
    int maxIterations = n * 3;
    int iter = 0;

    while (count > 3 && iter < maxIterations) {
        iter++;
        bool earFound = false;

        for (int i = 0; i < count; ++i) {
            int prevIdx = (i - 1 + count) % count;
            int currIdx = i;
            int nextIdx = (i + 1) % count;

            int u = indices[prevIdx];
            int v = indices[currIdx];
            int w = indices[nextIdx];

            if (isEar(u, v, w, indices, pts)) {
                triangles.append(u);
                triangles.append(v);
                triangles.append(w);

                indices.remove(currIdx);
                count--;
                earFound = true;
                break;
            }
        }

        // If precision issues prevented finding a strict ear, fallback to first non-collinear triplet
        if (!earFound && count > 3) {
            triangles.append(indices[0]);
            triangles.append(indices[1]);
            triangles.append(indices[2]);
            indices.remove(1);
            count--;
        }
    }

    if (count == 3) {
        triangles.append(indices[0]);
        triangles.append(indices[1]);
        triangles.append(indices[2]);
    }

    return triangles;
}

double Triangulator::calculateArea(const QPolygonF &polygon)
{
    if (polygon.size() < 3) return 0.0;
    double area = 0.0;
    int n = polygon.size();
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        area += polygon[i].x() * polygon[next].y() - polygon[next].x() * polygon[i].y();
    }
    return std::abs(area) * 0.5;
}

double Triangulator::calculateOverdrawSavings(const QPolygonF &polygon, const QSize &boundingBox)
{
    if (boundingBox.width() <= 0 || boundingBox.height() <= 0) {
        return 0.0;
    }

    double boxArea = static_cast<double>(boundingBox.width() * boundingBox.height());
    double polyArea = calculateArea(polygon);

    if (polyArea >= boxArea) {
        return 0.0;
    }

    double saved = (boxArea - polyArea) / boxArea * 100.0;
    return std::clamp(saved, 0.0, 100.0);
}

} // namespace BentoPackGeometry
