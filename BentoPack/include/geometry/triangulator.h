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

#ifndef TRIANGULATOR_H
#define TRIANGULATOR_H

#include <QPolygonF>
#include <QList>
#include <QSize>
#include <QImage>
#include "bentopackcore_export.h"

namespace BentoPackGeometry {

/**
 * @brief Mode of internal feature ridge detection (M19).
 */
enum class ContrastMode {
    RgbColorDistance = 0, ///< Euclidean RGB color distance / chromatic gradient (default for pixel art)
    SobelLuminance = 1    ///< Classical Sobel operator on scalar luminance
};

/**
 * @brief Parameters for intelligent polygonal mesh generation (M19).
 */
struct SmartMeshParams {
    int             steinerDensity = 30;          ///< 0 to 100: density of internal Steiner points (0 = off)
    double          minAngleDeg = 25.0;           ///< 15° to 35°: minimum angle target for quality triangles
    int             contrastSensitivity = 50;    ///< 0 to 100: edge detection sensitivity for internal feature ridges
    ContrastMode    contrastMode = ContrastMode::RgbColorDistance; ///< Color distance (default) or Sobel Luminance
    QList<QPointF>  userInteriorPoints;          ///< Manually placed interior points
};

/**
 * @brief Result of intelligent polygonal mesh generation (M19).
 */
struct SmartMeshResult {
    QPolygonF       outerPolygon;                 ///< Rigid boundary polygon
    QList<QPointF>  vertices;                     ///< All mesh vertices (boundary first, then interior points)
    QList<int>      triangles;                    ///< Vertex index triplets into vertices
    int             interiorVertexCount = 0;      ///< Count of interior Steiner / contrast vertices
};

/**
 * @brief Decomposes 2D polygons into triangles using Ear-Clipping and
 *        Constrained Delaunay Triangulation (CDT), and computes GPU metrics.
 */
class BENTOPACK_CORE_EXPORT Triangulator
{
public:
    /**
     * @brief Triangulates a simple 2D polygon into index triplets using Ear-Clipping.
     * @param polygon Input polygon (local coordinates).
     * @return List of vertex index triplets [i0, i1, i2, i3, i4, i5, ...].
     */
    static QList<int> triangulate(const QPolygonF &polygon);

    /**
     * @brief Performs Constrained Delaunay Triangulation (CDT) on a set of vertices
     *        constrained by a rigid outer boundary polygon (M19).
     * @param outerPolygon The rigid boundary polygon.
     * @param allVertices List of all vertices (must contain outerPolygon vertices at the start).
     * @return List of vertex index triplets into allVertices.
     */
    static QList<int> triangulateCDT(const QPolygonF &outerPolygon, const QList<QPointF> &allVertices);

    /**
     * @brief Generates an intelligent polygonal mesh (M19) with internal contrast edges
     *        and Steiner points for 2D animation / deformation rigging.
     * @param outerPolygon Base rigid boundary polygon (must already exist).
     * @param image Frame image used for contrast analysis.
     * @param params Smart mesh generation parameters.
     * @return SmartMeshResult containing outer polygon, all vertices, and triangulation.
     */
    static SmartMeshResult generateSmartMesh(const QPolygonF &outerPolygon,
                                             const QImage &image,
                                             const SmartMeshParams &params = SmartMeshParams());

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

} // namespace BentoPackGeometry

#endif // TRIANGULATOR_H
