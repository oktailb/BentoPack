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

namespace {

struct Triangle2D {
    int v0 = -1;
    int v1 = -1;
    int v2 = -1;

    bool containsVertex(int v) const {
        return v0 == v || v1 == v || v2 == v;
    }

    bool operator==(const Triangle2D &other) const {
        return v0 == other.v0 && v1 == other.v1 && v2 == other.v2;
    }

    bool hasEdge(int a, int b) const {
        return (v0 == a && v1 == b) || (v1 == a && v0 == b) ||
               (v1 == a && v2 == b) || (v2 == a && v1 == b) ||
               (v2 == a && v0 == b) || (v0 == a && v2 == b);
    }
};

struct Edge2D {
    int v0;
    int v1;
    bool operator==(const Edge2D &o) const {
        return (v0 == o.v0 && v1 == o.v1) || (v0 == o.v1 && v1 == o.v0);
    }
};

static bool circumcircleContains(const QPointF &a, const QPointF &b, const QPointF &c, const QPointF &p)
{
    double d = 2.0 * (a.x() * (b.y() - c.y()) + b.x() * (c.y() - a.y()) + c.x() * (a.y() - b.y()));
    if (std::abs(d) < 1e-12) return false;

    double a2 = a.x() * a.x() + a.y() * a.y();
    double b2 = b.x() * b.x() + b.y() * b.y();
    double c2 = c.x() * c.x() + c.y() * c.y();

    double ux = (a2 * (b.y() - c.y()) + b2 * (c.y() - a.y()) + c2 * (a.y() - b.y())) / d;
    double uy = (a2 * (c.x() - b.x()) + b2 * (a.x() - c.x()) + c2 * (b.x() - a.x())) / d;

    double r2 = (a.x() - ux) * (a.x() - ux) + (a.y() - uy) * (a.y() - uy);
    double dist2 = (p.x() - ux) * (p.x() - ux) + (p.y() - uy) * (p.y() - uy);

    return dist2 < r2 - 1e-9;
}

static bool segmentsIntersectStrict(const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4)
{
    auto ccw = [](const QPointF &a, const QPointF &b, const QPointF &c) {
        return (c.y() - a.y()) * (b.x() - a.x()) > (b.y() - a.y()) * (c.x() - a.x());
    };
    if (p1 == p3 || p1 == p4 || p2 == p3 || p2 == p4) return false;
    return (ccw(p1, p3, p4) != ccw(p2, p3, p4)) && (ccw(p1, p2, p3) != ccw(p1, p2, p4));
}

static double distancePointToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    double l2 = (b.x() - a.x()) * (b.x() - a.x()) + (b.y() - a.y()) * (b.y() - a.y());
    if (l2 < 1e-12) return std::hypot(p.x() - a.x(), p.y() - a.y());
    double t = std::clamp(((p.x() - a.x()) * (b.x() - a.x()) + (p.y() - a.y()) * (b.y() - a.y())) / l2, 0.0, 1.0);
    QPointF proj(a.x() + t * (b.x() - a.x()), a.y() + t * (b.y() - a.y()));
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

static double triangleMinAngle(const QPointF &a, const QPointF &b, const QPointF &c)
{
    double ab = std::hypot(b.x() - a.x(), b.y() - a.y());
    double bc = std::hypot(c.x() - b.x(), c.y() - b.y());
    double ca = std::hypot(a.x() - c.x(), a.y() - c.y());
    if (ab < 1e-9 || bc < 1e-9 || ca < 1e-9) return 0.0;

    auto angleBetween = [](double o1, double o2, double opp) {
        double cosVal = std::clamp((o1 * o1 + o2 * o2 - opp * opp) / (2.0 * o1 * o2), -1.0, 1.0);
        return std::acos(cosVal) * 180.0 / 3.14159265358979323846;
    };

    double angA = angleBetween(ab, ca, bc);
    double angB = angleBetween(ab, bc, ca);
    double angC = 180.0 - angA - angB;
    return std::min({angA, angB, angC});
}

} // namespace

QList<int> Triangulator::triangulateCDT(const QPolygonF &outerPolygon, const QList<QPointF> &allVertices)
{
    QPolygonF poly = outerPolygon;
    if (poly.size() > 1 && poly.first() == poly.last()) {
        poly.removeLast();
    }
    const int numBoundary = poly.size();
    if (numBoundary < 3 || allVertices.size() < numBoundary) {
        return {};
    }

    // If there are no interior vertices, fall back to robust Ear-Clipping
    if (allVertices.size() == numBoundary) {
        return triangulate(poly);
    }

    const int totalPoints = allVertices.size();

    // 1. Compute bounding box for super-triangle
    QRectF bounds = poly.boundingRect();
    for (const QPointF &p : allVertices) {
        bounds = bounds.united(QRectF(p, QSizeF(1, 1)));
    }
    double maxD = std::max(bounds.width(), bounds.height()) * 10.0 + 100.0;
    QPointF center = bounds.center();

    QList<QPointF> pts = allVertices;
    pts.append(QPointF(center.x() - maxD, center.y() - maxD));
    pts.append(QPointF(center.x() + maxD, center.y() - maxD));
    pts.append(QPointF(center.x(), center.y() + maxD * 1.5));

    const int s0 = totalPoints;
    const int s1 = totalPoints + 1;
    const int s2 = totalPoints + 2;

    QList<Triangle2D> mesh;
    mesh.append({s0, s1, s2});

    // 2. Incremental Delaunay insertion
    for (int i = 0; i < totalPoints; ++i) {
        const QPointF &p = pts[i];
        QList<Triangle2D> badTriangles;
        QList<Edge2D> polygonEdges;

        for (const Triangle2D &tri : mesh) {
            if (circumcircleContains(pts[tri.v0], pts[tri.v1], pts[tri.v2], p)) {
                badTriangles.append(tri);
                Edge2D e1{tri.v0, tri.v1};
                Edge2D e2{tri.v1, tri.v2};
                Edge2D e3{tri.v2, tri.v0};
                for (const Edge2D &e : {e1, e2, e3}) {
                    int idx = polygonEdges.indexOf(e);
                    if (idx >= 0) {
                        polygonEdges.removeAt(idx);
                    } else {
                        polygonEdges.append(e);
                    }
                }
            }
        }

        for (const Triangle2D &tri : badTriangles) {
            mesh.removeOne(tri);
        }

        for (const Edge2D &e : polygonEdges) {
            mesh.append({e.v0, e.v1, i});
        }
    }

    // 3. Remove super-triangle vertices
    for (int i = mesh.size() - 1; i >= 0; --i) {
        const Triangle2D &tri = mesh[i];
        if (tri.containsVertex(s0) || tri.containsVertex(s1) || tri.containsVertex(s2)) {
            mesh.removeAt(i);
        }
    }

    // 4. Enforce outer boundary constraint edges via Lawson diagonal flips
    for (int b = 0; b < numBoundary; ++b) {
        int vA = b;
        int vB = (b + 1) % numBoundary;
        const QPointF &pA = pts[vA];
        const QPointF &pB = pts[vB];

        int flipLimit = 50;
        while (flipLimit-- > 0) {
            bool hasBoundaryEdge = false;
            for (const Triangle2D &tri : mesh) {
                if (tri.hasEdge(vA, vB)) {
                    hasBoundaryEdge = true;
                    break;
                }
            }
            if (hasBoundaryEdge) break;

            // Find an intersecting triangle edge
            bool flipped = false;
            for (int t1 = 0; t1 < mesh.size(); ++t1) {
                const Triangle2D &tri1 = mesh[t1];
                for (int t2 = t1 + 1; t2 < mesh.size(); ++t2) {
                    const Triangle2D &tri2 = mesh[t2];

                    // Check if they share an edge
                    int sharedU = -1, sharedW = -1;
                    int opp1 = -1, opp2 = -1;
                    int verts1[3] = {tri1.v0, tri1.v1, tri1.v2};
                    int verts2[3] = {tri2.v0, tri2.v1, tri2.v2};

                    for (int u : verts1) {
                        if (tri2.containsVertex(u)) {
                            if (sharedU == -1) sharedU = u;
                            else sharedW = u;
                        } else {
                            opp1 = u;
                        }
                    }
                    for (int u : verts2) {
                        if (!tri1.containsVertex(u)) {
                            opp2 = u;
                        }
                    }

                    if (sharedU != -1 && sharedW != -1 && opp1 != -1 && opp2 != -1) {
                        if (segmentsIntersectStrict(pA, pB, pts[sharedU], pts[sharedW])) {
                            // Check if quadrilateral is strictly convex
                            if (segmentsIntersectStrict(pts[opp1], pts[opp2], pts[sharedU], pts[sharedW])) {
                                mesh[t1] = {sharedU, opp1, opp2};
                                mesh[t2] = {sharedW, opp1, opp2};
                                flipped = true;
                                break;
                            }
                        }
                    }
                }
                if (flipped) break;
            }
            if (!flipped) break;
        }
    }

    // 5. Prune triangles whose centroid is outside the outer boundary polygon
    QList<int> resultTriangles;
    resultTriangles.reserve(mesh.size() * 3);

    for (const Triangle2D &tri : mesh) {
        if (tri.v0 < 0 || tri.v0 >= totalPoints ||
            tri.v1 < 0 || tri.v1 >= totalPoints ||
            tri.v2 < 0 || tri.v2 >= totalPoints) continue;

        const QPointF &p0 = pts[tri.v0];
        const QPointF &p1 = pts[tri.v1];
        const QPointF &p2 = pts[tri.v2];

        QPointF centroid((p0.x() + p1.x() + p2.x()) / 3.0,
                         (p0.y() + p1.y() + p2.y()) / 3.0);

        if (!poly.containsPoint(centroid, Qt::OddEvenFill)) {
            continue;
        }

        // Also check that none of the edges cross the boundary
        bool edgeCrosses = false;
        const QPointF triVerts[3] = {p0, p1, p2};
        for (int e = 0; e < 3; ++e) {
            const QPointF &eA = triVerts[e];
            const QPointF &eB = triVerts[(e + 1) % 3];
            for (int b = 0; b < numBoundary; ++b) {
                if (segmentsIntersectStrict(eA, eB, poly[b], poly[(b + 1) % numBoundary])) {
                    edgeCrosses = true;
                    break;
                }
            }
            if (edgeCrosses) break;
        }

        if (edgeCrosses) continue;

        resultTriangles.append(tri.v0);
        resultTriangles.append(tri.v1);
        resultTriangles.append(tri.v2);
    }

    return resultTriangles;
}

SmartMeshResult Triangulator::generateSmartMesh(const QPolygonF &outerPolygon,
                                                const QImage &image,
                                                const SmartMeshParams &params)
{
    SmartMeshResult result;
    QPolygonF poly = outerPolygon;
    if (poly.size() > 1 && poly.first() == poly.last()) {
        poly.removeLast();
    }
    if (poly.size() < 3) {
        return result;
    }

    result.outerPolygon = poly;

    // Start with outer boundary vertices
    QList<QPointF> allVertices = poly.toList();

    // Add any user-specified interior points that lie strictly inside
    for (const QPointF &up : params.userInteriorPoints) {
        if (poly.containsPoint(up, Qt::OddEvenFill)) {
            bool duplicate = false;
            for (const QPointF &v : allVertices) {
                if (std::hypot(v.x() - up.x(), v.y() - up.y()) < 2.0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                allVertices.append(up);
            }
        }
    }

    const int imgW = image.width();
    const int imgH = image.height();
    const double polyArea = calculateArea(poly);

    // 1. Feature Edge Detection (Sobel on Luminance)
    if (params.contrastSensitivity > 0 && !image.isNull() && imgW > 2 && imgH > 2) {
        const double threshold = std::clamp(280.0 - (params.contrastSensitivity * 2.2), 30.0, 260.0);
        const int minFeatureSpacing = std::clamp(static_cast<int>(std::round(std::sqrt(polyArea) * 0.08)), 4, 12);

        QList<QPointF> featureCandidates;
        QRect polyRect = poly.boundingRect().toRect().adjusted(-1, -1, 1, 1);
        int startX = std::max(1, polyRect.left());
        int endX = std::min(imgW - 2, polyRect.right());
        int startY = std::max(1, polyRect.top());
        int endY = std::min(imgH - 2, polyRect.bottom());

        for (int y = startY; y <= endY; ++y) {
            for (int x = startX; x <= endX; ++x) {
                QPointF pt(x + 0.5, y + 0.5);
                if (!poly.containsPoint(pt, Qt::OddEvenFill)) continue;

                // Check distance to boundary edges
                double distToBoundary = 1e9;
                for (int b = 0; b < poly.size(); ++b) {
                    distToBoundary = std::min(distToBoundary,
                        distancePointToSegment(pt, poly[b], poly[(b + 1) % poly.size()]));
                }
                if (distToBoundary < 3.0) continue;

                // Check opacity
                QRgb centerRgb = image.pixel(x, y);
                if (qAlpha(centerRgb) < 64) continue;

                auto lum = [&](int px, int py) -> double {
                    QRgb c = image.pixel(px, py);
                    return 0.299 * qRed(c) + 0.587 * qGreen(c) + 0.114 * qBlue(c);
                };

                double gx = (lum(x + 1, y - 1) + 2.0 * lum(x + 1, y) + lum(x + 1, y + 1)) -
                            (lum(x - 1, y - 1) + 2.0 * lum(x - 1, y) + lum(x - 1, y + 1));
                double gy = (lum(x - 1, y + 1) + 2.0 * lum(x, y + 1) + lum(x + 1, y + 1)) -
                            (lum(x - 1, y - 1) + 2.0 * lum(x, y - 1) + lum(x + 1, y - 1));

                double grad = std::hypot(gx, gy);
                if (grad >= threshold) {
                    // Check spacing with existing points
                    bool tooClose = false;
                    for (const QPointF &v : allVertices) {
                        if (std::hypot(v.x() - pt.x(), v.y() - pt.y()) < minFeatureSpacing) {
                            tooClose = true;
                            break;
                        }
                    }
                    if (!tooClose) {
                        for (const QPointF &cand : featureCandidates) {
                            if (std::hypot(cand.x() - pt.x(), cand.y() - pt.y()) < minFeatureSpacing) {
                                tooClose = true;
                                break;
                            }
                        }
                    }
                    if (!tooClose) {
                        featureCandidates.append(pt);
                    }
                }
            }
        }
        allVertices.append(featureCandidates);
    }

    // 2. Steiner Point Insertion for Delaunay Angle & Density Quality
    if (params.steinerDensity > 0) {
        // Compute target max triangle area
        double densityFactor = std::clamp(params.steinerDensity / 100.0, 0.05, 1.0);
        double maxAllowedArea = std::max(16.0, polyArea / (3.0 + densityFactor * 25.0));
        int maxSteinerIters = std::clamp(static_cast<int>(densityFactor * 35.0), 5, 40);

        for (int iter = 0; iter < maxSteinerIters; ++iter) {
            QList<int> tris = triangulateCDT(poly, allVertices);
            if (tris.isEmpty()) break;

            int worstTriIdx = -1;
            double worstScore = 0.0;
            QPointF worstCentroid;

            for (int t = 0; t + 2 < tris.size(); t += 3) {
                const QPointF &p0 = allVertices[tris[t]];
                const QPointF &p1 = allVertices[tris[t + 1]];
                const QPointF &p2 = allVertices[tris[t + 2]];

                QPolygonF triPoly{p0, p1, p2};
                double area = calculateArea(triPoly);
                double minAng = triangleMinAngle(p0, p1, p2);

                bool needsRefinement = (area > maxAllowedArea) || (minAng < params.minAngleDeg);
                if (needsRefinement) {
                    double score = (area / maxAllowedArea) + (params.minAngleDeg - minAng);
                    if (score > worstScore) {
                        QPointF c((p0.x() + p1.x() + p2.x()) / 3.0, (p0.y() + p1.y() + p2.y()) / 3.0);
                        if (poly.containsPoint(c, Qt::OddEvenFill)) {
                            // Check distance to boundary
                            double bDist = 1e9;
                            for (int b = 0; b < poly.size(); ++b) {
                                bDist = std::min(bDist, distancePointToSegment(c, poly[b], poly[(b + 1) % poly.size()]));
                            }
                            if (bDist >= 2.5) {
                                // Check distance to existing vertices
                                double vDist = 1e9;
                                for (const QPointF &v : allVertices) {
                                    vDist = std::min(vDist, std::hypot(v.x() - c.x(), v.y() - c.y()));
                                }
                                if (vDist >= 3.0) {
                                    worstScore = score;
                                    worstTriIdx = t;
                                    worstCentroid = c;
                                }
                            }
                        }
                    }
                }
            }

            if (worstTriIdx >= 0) {
                allVertices.append(worstCentroid);
            } else {
                break;
            }
        }
    }

    // Final triangulation
    result.vertices = allVertices;
    result.interiorVertexCount = std::max(0, static_cast<int>(allVertices.size()) - static_cast<int>(poly.size()));
    result.triangles = triangulateCDT(poly, allVertices);

    return result;
}

} // namespace BentoPackGeometry
