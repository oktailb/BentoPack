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

#ifndef POLYGONSIMPLIFIER_H
#define POLYGONSIMPLIFIER_H

#include <QPolygonF>
#include <QSize>
#include "bentopackcore_export.h"

namespace BentoPackGeometry {

/**
 * @brief Simplifies 2D boundary polygons using the Ramer-Douglas-Peucker (RDP) algorithm,
 *        with outward normal dilation to guarantee that no opaque pixel is cropped.
 */
class BENTOPACK_CORE_EXPORT PolygonSimplifier
{
public:
    /**
     * @brief Simplifies a raw contour into an optimized polygonal envelope.
     * @param rawContour Raw high-density contour points.
     * @param epsilon Distance tolerance in pixels (higher = fewer vertices).
     * @param padding Outward expansion in pixels to avoid clipping edge pixels.
     * @param maxVertices Strict upper limit on vertex count (e.g. 12 or 16).
     * @param bounds Frame dimensions for clamping (optional).
     * @return Simplified and padded QPolygonF.
     */
    static QPolygonF simplify(const QPolygonF &rawContour,
                              double epsilon = 1.5,
                              double padding = 1.0,
                              int maxVertices = 16,
                              const QSize &bounds = QSize());
};

} // namespace BentoPackGeometry

#endif // POLYGONSIMPLIFIER_H
