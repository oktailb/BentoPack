#ifndef POLYGONSIMPLIFIER_H
#define POLYGONSIMPLIFIER_H

#include <QPolygonF>
#include <QSize>
#include "spritestudiocore_export.h"

namespace SpriteStudioGeometry {

/**
 * @brief Simplifies 2D boundary polygons using the Ramer-Douglas-Peucker (RDP) algorithm,
 *        with outward normal dilation to guarantee that no opaque pixel is cropped.
 */
class SPRITESTUDIO_CORE_EXPORT PolygonSimplifier
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

} // namespace SpriteStudioGeometry

#endif // POLYGONSIMPLIFIER_H
