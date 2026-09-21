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

#ifndef TRIANGULATOR_H
#define TRIANGULATOR_H

#include <QPolygonF>
#include <QList>
#include <QSize>
#include "spritestudiocore_export.h"

namespace SpriteStudioGeometry {

/**
 * @brief Decomposes simple 2D polygons into triangles using Ear-Clipping,
 *        and computes GPU overdraw reduction metrics.
 */
class SPRITESTUDIO_CORE_EXPORT Triangulator
{
public:
    /**
     * @brief Triangulates a simple 2D polygon into index triplets.
     * @param polygon Input polygon (local coordinates).
     * @return List of vertex index triplets [i0, i1, i2, i3, i4, i5, ...].
     */
    static QList<int> triangulate(const QPolygonF &polygon);

    /**
     * @brief Computes the surface area of a polygon using the Shoelace formula.
     */
    static double calculateArea(const QPolygonF &polygon);

    /**
     * @brief Computes the percentage of transparent pixels eliminated by the polygon
     *        compared to the rectangular bounding box (Fillrate / Overdraw savings).
     * @param polygon Area-defining polygon.
     * @param boundingBox Dimensions of the original rectangular sprite box.
     * @return Value between 0.0% and 100.0%.
     */
    static double calculateOverdrawSavings(const QPolygonF &polygon, const QSize &boundingBox);
};

} // namespace SpriteStudioGeometry

#endif // TRIANGULATOR_H
