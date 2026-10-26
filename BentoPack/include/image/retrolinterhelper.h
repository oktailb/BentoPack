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

#ifndef RETROLINTERHELPER_H
#define RETROLINTERHELPER_H

#include <QImage>
#include <QList>
#include <QString>
#include "bentopackcore_export.h"
#include "config/appconfig.h"

/**
 * @brief High-performance helper analyzing hardware PPU constraints and unique VRAM tiles.
 */
class BENTOPACK_CORE_EXPORT RetroLinterHelper
{
public:
    /**
     * @brief Computes the count of unique non-empty tiles (default 8x8) across frames.
     */
    static int countUniqueTiles(const QList<QImage> &frames, int tileSize = 8);

    /**
     * @brief Checks if a frame exceeds the active scanline sprite width constraint.
     */
    static bool isScanlineViolated(const QImage &frame, const RetroLinterConfig &cfg);

    /**
     * @brief Returns a localized warning string if the frame exceeds scanline constraints.
     */
    static QString scanlineViolationMessage(const QImage &frame, const RetroLinterConfig &cfg);
};

#endif // RETROLINTERHELPER_H
