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

#include "geometry/polygonmerger.h"
#include "geometry/contourtracer.h"
#include "geometry/polygonsimplifier.h"
#include "geometry/triangulator.h"
#include "model/spritedocument.h"
#include <QPainterPath>
#include <cmath>
#include <limits>
#include <algorithm>

namespace BentoPackGeometry {

namespace {

static QPointF closestPointOnSegment(const QPointF &pt, const QPointF &segA, const QPointF &segB)
{
    QPointF ab = segB - segA;
    double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-9) {
        return segA;
    }
    double t = ((pt.x() - segA.x()) * ab.x() + (pt.y() - segA.y()) * ab.y()) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    return segA + t * ab;
}

} // namespace

QPolygonF PolygonMerger::mergePolygons(const QPolygonF &polyA,
                                      const QPolygonF &polyB,
                                      double bridgeWidth)
{
    if (polyA.isEmpty()) return polyB;
    if (polyB.isEmpty()) return polyA;

    QPolygonF cleanA = polyA;
    if (cleanA.size() > 1 && cleanA.first() == cleanA.last()) {
        cleanA.removeLast();
    }
    QPolygonF cleanB = polyB;
    if (cleanB.size() > 1 && cleanB.first() == cleanB.last()) {
        cleanB.removeLast();
    }

    if (cleanA.size() < 3) return polyB;
    if (cleanB.size() < 3) return polyA;

    QPainterPath pathA;
    pathA.addPolygon(cleanA);
    QPainterPath pathB;
    pathB.addPolygon(cleanB);

    QPainterPath united = pathA.united(pathB);
    QList<QPolygonF> subs = united.toSubpathPolygons();

    // If already intersecting or touching, directly return the unified outer contour
    if (subs.size() <= 1 && !subs.isEmpty()) {
        QPolygonF res = subs.first();
        if (res.size() >= 3) {
            return res;
        }
        return united.toFillPolygon();
    }

    // Polygons are disjoint: compute shortest distance pair (bestA, bestB)
    double minDistSq = std::numeric_limits<double>::max();
    QPointF bestA;
    QPointF bestB;

    int nA = cleanA.size();
    int nB = cleanB.size();

    for (int i = 0; i < nA; ++i) {
        const QPointF &pA = cleanA[i];
        for (int j = 0; j < nB; ++j) {
            const QPointF &sB0 = cleanB[j];
            const QPointF &sB1 = cleanB[(j + 1) % nB];
            QPointF candB = closestPointOnSegment(pA, sB0, sB1);
            double dSq = (pA.x() - candB.x()) * (pA.x() - candB.x()) + (pA.y() - candB.y()) * (pA.y() - candB.y());
            if (dSq < minDistSq) {
                minDistSq = dSq;
                bestA = pA;
                bestB = candB;
            }
        }
    }

    for (int j = 0; j < nB; ++j) {
        const QPointF &pB = cleanB[j];
        for (int i = 0; i < nA; ++i) {
            const QPointF &sA0 = cleanA[i];
            const QPointF &sA1 = cleanA[(i + 1) % nA];
            QPointF candA = closestPointOnSegment(pB, sA0, sA1);
            double dSq = (candA.x() - pB.x()) * (candA.x() - pB.x()) + (candA.y() - pB.y()) * (candA.y() - pB.y());
            if (dSq < minDistSq) {
                minDistSq = dSq;
                bestA = candA;
                bestB = pB;
            }
        }
    }

    QRectF unionBounds = cleanA.boundingRect().united(cleanB.boundingRect());

    double dx = bestB.x() - bestA.x();
    double dy = bestB.y() - bestA.y();
    double dist = std::sqrt(dx * dx + dy * dy);

    // If touching or overlapping, find center of the contact interface
    if (dist <= 1.0) {
        QPointF contactCenter(0, 0);
        int contactCount = 0;
        for (int i = 0; i < nA; ++i) {
            const QPointF &pA = cleanA[i];
            for (int j = 0; j < nB; ++j) {
                const QPointF &sB0 = cleanB[j];
                const QPointF &sB1 = cleanB[(j + 1) % nB];
                QPointF candB = closestPointOnSegment(pA, sB0, sB1);
                double dSq = (pA.x() - candB.x()) * (pA.x() - candB.x()) + (pA.y() - candB.y()) * (pA.y() - candB.y());
                if (dSq <= 1.0) {
                    contactCenter += (pA + candB) * 0.5;
                    contactCount++;
                }
            }
        }
        if (contactCount > 0) {
            bestA = contactCenter / static_cast<double>(contactCount);
            bestB = bestA;
        }
    }

    // Calculate centroids for directional fallback if touching on boundary
    QPointF centA(0, 0);
    for (const QPointF &pt : cleanA) centA += pt;
    centA /= static_cast<double>(cleanA.size());

    QPointF centB(0, 0);
    for (const QPointF &pt : cleanB) centB += pt;
    centB /= static_cast<double>(cleanB.size());

    if (dist <= 1e-4) {
        dx = centB.x() - centA.x();
        dy = centB.y() - centA.y();
    }
    double dirLen = std::sqrt(dx * dx + dy * dy);

    QPainterPath bridgePath;
    if (dirLen > 1e-6) {
        double dirX = dx / dirLen;
        double dirY = dy / dirLen;
        double normX = -dirY;
        double normY = dirX;

        double halfW = std::max(0.5, bridgeWidth / 2.0);
        double ext = (dist > 1e-4) ? std::min(2.0, std::max(0.5, dist * 0.25)) : 1.0;

        auto clampToUnion = [&unionBounds](const QPointF &pt) {
            return QPointF(std::clamp(pt.x(), unionBounds.left(), unionBounds.right()),
                           std::clamp(pt.y(), unionBounds.top(), unionBounds.bottom()));
        };

        QPointF p0 = clampToUnion(QPointF(bestA.x() - dirX * ext - normX * halfW, bestA.y() - dirY * ext - normY * halfW));
        QPointF p1 = clampToUnion(QPointF(bestA.x() - dirX * ext + normX * halfW, bestA.y() - dirY * ext + normY * halfW));
        QPointF p2 = clampToUnion(QPointF(bestB.x() + dirX * ext + normX * halfW, bestB.y() + dirY * ext + normY * halfW));
        QPointF p3 = clampToUnion(QPointF(bestB.x() + dirX * ext - normX * halfW, bestB.y() + dirY * ext - normY * halfW));

        QPolygonF bridgeQuad;
        bridgeQuad << p0 << p1 << p2 << p3;
        bridgePath.addPolygon(bridgeQuad);
    }

    QPainterPath full = pathA.united(pathB);
    if (!bridgePath.isEmpty()) {
        full = full.united(bridgePath);
    }

    QList<QPolygonF> fusedSubs = full.toSubpathPolygons();
    QPolygonF resultPoly;
    double maxArea = -1.0;
    for (const QPolygonF &sp : fusedSubs) {
        double a = std::abs(Triangulator::calculateArea(sp));
        if (a > maxArea) {
            maxArea = a;
            resultPoly = sp;
        }
    }

    if (resultPoly.isEmpty()) {
        resultPoly = full.toFillPolygon();
    }

    if (resultPoly.size() >= 3) {
        // Prune collinear points created along bridge corridor without altering geometry
        QPolygonF simplified = PolygonSimplifier::simplify(resultPoly, 0.75, 0.0, 64);
        if (simplified.size() >= 3) {
            resultPoly = simplified;
        }
    }

    return resultPoly;
}

QPolygonF PolygonMerger::mergeMultiplePolygons(const QList<QPolygonF> &polygons, double bridgeWidth)
{
    QList<QPolygonF> validPolys;
    for (const QPolygonF &p : polygons) {
        if (p.size() >= 3) {
            validPolys.append(p);
        }
    }

    if (validPolys.isEmpty()) return QPolygonF();
    if (validPolys.size() == 1) return validPolys.first();

    // Greedily merge the two closest polygons until 1 remains (Minimum Spanning Tree order)
    while (validPolys.size() > 1) {
        double bestDistSq = std::numeric_limits<double>::max();
        int bestIdxA = 0;
        int bestIdxB = 1;

        for (int i = 0; i < validPolys.size(); ++i) {
            for (int j = i + 1; j < validPolys.size(); ++j) {
                const QPolygonF &pA = validPolys[i];
                const QPolygonF &pB = validPolys[j];
                for (int va = 0; va < pA.size(); ++va) {
                    for (int vb = 0; vb < pB.size(); ++vb) {
                        double dSq = (pA[va].x() - pB[vb].x()) * (pA[va].x() - pB[vb].x())
                                   + (pA[va].y() - pB[vb].y()) * (pA[va].y() - pB[vb].y());
                        if (dSq < bestDistSq) {
                            bestDistSq = dSq;
                            bestIdxA = i;
                            bestIdxB = j;
                        }
                    }
                }
            }
        }

        QPolygonF merged = mergePolygons(validPolys[bestIdxA], validPolys[bestIdxB], bridgeWidth);
        if (bestIdxA > bestIdxB) {
            std::swap(bestIdxA, bestIdxB);
        }
        validPolys.removeAt(bestIdxB);
        validPolys.removeAt(bestIdxA);
        validPolys.append(merged);
    }

    return validPolys.first();
}

SpriteBox PolygonMerger::mergeSpriteBoxes(const SpriteBox &srcBox,
                                         const SpriteBox &tgtBox,
                                         const QImage &srcImage,
                                         const QImage &tgtImage)
{
    SpriteBox newBox;
    QRect unitedRect = srcBox.rect.united(tgtBox.rect);
    newBox.rect = unitedRect;
    newBox.selected = srcBox.selected || tgtBox.selected;
    newBox.index = tgtBox.index;

    // Pivot preservation
    if (tgtBox.hasCustomPivot) {
        newBox.hasCustomPivot = true;
        newBox.pivot = (tgtBox.rect.topLeft() + tgtBox.pivot) - unitedRect.topLeft();
    } else if (srcBox.hasCustomPivot) {
        newBox.hasCustomPivot = true;
        newBox.pivot = (srcBox.rect.topLeft() + srcBox.pivot) - unitedRect.topLeft();
    }

    // Polygon mesh merging
    bool hasMesh = srcBox.hasPolygonMesh || tgtBox.hasPolygonMesh;
    if (hasMesh) {
        QPolygonF polyA = srcBox.polygon;
        if (polyA.isEmpty() && !srcImage.isNull()) {
            QPolygonF raw = ContourTracer::traceContour(srcImage, 10);
            polyA = PolygonSimplifier::simplify(raw, 1.5, 1.0, 32, srcImage.size());
        }

        QPolygonF polyB = tgtBox.polygon;
        if (polyB.isEmpty() && !tgtImage.isNull()) {
            QPolygonF raw = ContourTracer::traceContour(tgtImage, 10);
            polyB = PolygonSimplifier::simplify(raw, 1.5, 1.0, 32, tgtImage.size());
        }

        QPoint offsetA = srcBox.rect.topLeft() - unitedRect.topLeft();
        QPoint offsetB = tgtBox.rect.topLeft() - unitedRect.topLeft();

        QPolygonF shiftedA = polyA.translated(offsetA);
        QPolygonF shiftedB = polyB.translated(offsetB);

        QPolygonF mergedPoly = mergePolygons(shiftedA, shiftedB, 2.0);
        if (mergedPoly.size() >= 3) {
            newBox.hasPolygonMesh = true;
            newBox.polygon = mergedPoly;
            newBox.vertices = mergedPoly.toList();
            newBox.triangles = Triangulator::triangulate(mergedPoly);
        }
    }

    return newBox;
}

} // namespace BentoPackGeometry
