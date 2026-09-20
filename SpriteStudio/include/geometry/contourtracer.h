#ifndef CONTOURTRACER_H
#define CONTOURTRACER_H

#include <QImage>
#include <QPolygonF>
#include <QList>
#include "spritestudiocore_export.h"

namespace SpriteStudioGeometry {

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

} // namespace SpriteStudioGeometry

#endif // CONTOURTRACER_H
