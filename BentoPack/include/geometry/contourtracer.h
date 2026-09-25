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

#ifndef CONTOURTRACER_H
#define CONTOURTRACER_H

#include <QImage>
#include <QPolygonF>
#include <QList>
#include "bentopackcore_export.h"

namespace BentoPackGeometry {

/**
 * @brief Utility for extracting 2D boundary contours from sprite images
 *        based on the alpha channel.
 */
class SPRITESTUDIO_CORE_EXPORT ContourTracer
{
public:
    /**
     * @brief Extracts the outer boundary polygon of opaque pixels in the image.
     * @param image Input image (assumed RGBA/ARGB32).
     * @param alphaThreshold Opacity threshold [1..255] above which pixels are considered solid.
     * @return Closed QPolygonF representing the outer silhouette in image-local coordinates.
     */
    static QPolygonF traceContour(const QImage &image, int alphaThreshold = 10);

    /**
     * @brief Extracts all boundary contours (including holes and disjoint islands).
     */
    static QList<QPolygonF> traceAllContours(const QImage &image, int alphaThreshold = 10);
};

} // namespace BentoPackGeometry

#endif // CONTOURTRACER_H
