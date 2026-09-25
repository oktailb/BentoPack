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

#ifndef POLYGONMERGER_H
#define POLYGONMERGER_H

#include "bentopackcore_export.h"
#include <QPolygonF>
#include <QList>
#include <QRect>
#include <QImage>

struct SpriteBox;

namespace BentoPackGeometry {

/**
 * @brief Fuses two or more 2D sprite polygons or bounding boxes into a unified
 *        boundary polygon that minimizes empty space without degenerating into a convex hull.
 */
class BENTOPACK_CORE_EXPORT PolygonMerger
{
public:
    /**
     * @brief Merges two polygons defined in a common coordinate space.
     *        If disjoint, connects them using a minimal bridge corridor between
     *        their closest boundary features to preserve all concavities.
     * @param polyA First polygon.
     * @param polyB Second polygon.
     * @param bridgeWidth Corridor width in pixels when bridging disjoint polygons (default: 2.0).
     * @return Tight non-convex unified polygon.
     */
    static QPolygonF mergePolygons(const QPolygonF &polyA,
                                  const QPolygonF &polyB,
                                  double bridgeWidth = 2.0);

    /**
     * @brief Merges multiple polygons into a single non-convex unified polygon.
     * @param polygons List of input polygons.
     * @param bridgeWidth Corridor width in pixels.
     * @return Unified non-convex polygon.
     */
    static QPolygonF mergeMultiplePolygons(const QList<QPolygonF> &polygons,
                                          double bridgeWidth = 2.0);

    /**
     * @brief Computes the merged SpriteBox and polygon mesh for two merged sprite frames.
     *        Adjusts local coordinates relative to the united bounding rectangle.
     * @param srcBox Source box.
     * @param tgtBox Target box.
     * @param srcImage Source frame image.
     * @param tgtImage Target frame image.
     * @return Updated SpriteBox with merged rect, pivot, polygon, vertices, and triangulation.
     */
    static SpriteBox mergeSpriteBoxes(const SpriteBox &srcBox,
                                     const SpriteBox &tgtBox,
                                     const QImage &srcImage,
                                     const QImage &tgtImage);
};

} // namespace BentoPackGeometry

#endif // POLYGONMERGER_H
