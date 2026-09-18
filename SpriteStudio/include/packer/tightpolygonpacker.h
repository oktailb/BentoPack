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
