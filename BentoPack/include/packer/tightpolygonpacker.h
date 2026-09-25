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

#ifndef TIGHTPOLYGONPACKER_H
#define TIGHTPOLYGONPACKER_H

#include <QImage>
#include <QList>
#include <QPolygonF>
#include <QRect>
#include "packer/atlaspacker.h"

/**
 * @brief High-density 2D bin-packer supporting concave & non-rectangular polygon nesting.
 *
 * Allows bounding boxes (AABBs) to overlap as long as actual sprite polygons
 * (plus padding and extrusion) do not collide, resulting in 25%-50% smaller atlas sizes.
 */
class TightPolygonPacker
{
public:
    static AtlasPackResult pack(const QList<QImage> &uniqueFrames,
                                const QList<int> &mapping,
                                const AtlasPacker::PackOptions &options,
                                const QList<QPolygonF> &uniquePolygons = {});
};

#endif // TIGHTPOLYGONPACKER_H
