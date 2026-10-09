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

#ifndef MULTIATLASPACKER_H
#define MULTIATLASPACKER_H

#include <QImage>
#include <QMap>
#include <QList>
#include <QString>
#include <QColor>
#include "packer/atlaspacker.h"
#include "bentopackcore_export.h"

class SpriteDocument;

/**
 * @brief Type of texture/material channel for 2D lighting and dynamic shading.
 */
enum class MaterialMapType {
    Albedo = 0,     ///< Primary diffuse / color map (Master)
    Normal,         ///< Tangent-space normal map (_n / _normal)
    Emissive,       ///< Self-illumination glow map (_e / _emissive)
    Specular        ///< Specular reflection / roughness map (_s / _specular)
};

/**
 * @brief Bundled result of a synchronized multi-atlas packing pass.
 */
struct BENTOPACK_CORE_EXPORT MultiAtlasPackResult {
    AtlasPackResult                 master;             ///< Master Albedo pack result with UVs & layout
    QMap<MaterialMapType, QImage>   auxiliaryAtlases;   ///< Synchronized secondary textures (Normal, Emissive, Specular)
    QMap<MaterialMapType, QImage>   maps;               ///< Direct map alias for easy access
    bool                            success = false;

    bool hasMap(MaterialMapType type) const {
        return (auxiliaryAtlases.contains(type) && !auxiliaryAtlases.value(type).isNull())
            || (maps.contains(type) && !maps.value(type).isNull());
    }
};

/**
 * @brief Coordinates synchronized master-slave packing for 2D lighting textures.
 *
 * Automatically detects layer/file naming conventions (_n, _e, _s), applies identical
 * geometric transformations (MaxRects, rotations, padding, extrusion, CDT polygon meshes)
 * from the master diffuse texture to companion normal, emissive, and specular atlases.
 */
class BENTOPACK_CORE_EXPORT MultiAtlasPacker
{
public:
    /**
     * @brief Detects the material map type from a layer or file name suffix.
     */
    static MaterialMapType detectMapType(const QString &name);

    /**
     * @brief Returns the canonical filename suffix for a given map type (e.g. "_n", "_e", "_s").
     */
    static QString mapTypeSuffix(MaterialMapType type);

    /**
     * @brief Returns a human-readable display name for the map type.
     */
    static QString mapTypeName(MaterialMapType type);

    /**
     * @brief Returns the industry standard neutral background / fill color for a given map type.
     * Normal: tangent-space neutral RGBA(128, 128, 255, 0).
     * Emissive & Specular: transparent black RGBA(0, 0, 0, 0).
     */
    static QColor neutralColor(MaterialMapType type);

    /**
     * @brief Flips the green channel (Y-axis) of a normal map for OpenGL (Y+) vs DirectX/Unity/Unreal (Y-).
     */
    static QImage flipNormalMapY(const QImage &normalMap);

    /**
     * @brief Generates a synchronized slave atlas using the exact placement and dimensions of masterResult.
     */
    static QImage generateSlaveAtlas(const QList<QImage> &slaveFrames,
                                     const AtlasPackResult &masterResult,
                                     MaterialMapType mapType,
                                     int extrude = 0,
                                     bool normalMapYFlip = false);

    /**
     * @brief Performs full multi-atlas packing on a SpriteDocument.
     * Computes the master Albedo layout, then generates and synchronizes all active auxiliary maps.
     */
    static MultiAtlasPackResult pack(const SpriteDocument &doc,
                                     const AtlasPacker::PackOptions &options,
                                     bool normalMapYFlip = false);
};

#endif // MULTIATLASPACKER_H
