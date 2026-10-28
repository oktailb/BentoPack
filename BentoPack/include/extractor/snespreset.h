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

#ifndef SNESPRESET_H
#define SNESPRESET_H

#include "bentopackcore_export.h"
#include "extractor/retrotiledecoder.h"
#include <QString>
#include <QList>

struct BENTOPACK_CORE_EXPORT SnesPreset
{
    QString     title;
    QString     gameName;
    QString     category;
    bool        isHiRom = false;
    int         tileOffset = 0;       // PC physical byte offset
    int         tileLength = 16384;   // Size in bytes to decode
    int         bpp = 4;              // Typically 4bpp on SNES (or 3bpp, 2bpp)
    TilePattern pattern = TilePattern::Linear;
    int         tilesPerRow = 16;
    int         paletteOffset = -1;   // PC byte offset for 15-bit BGR555 palette (-1 = from savestate/auto)
    int         defaultPaletteIndex = 8; // Default active OBJ palette (8-15)
    QString     description;

    bool isValid() const { return !title.isEmpty() && tileLength > 0; }
};

class BENTOPACK_CORE_EXPORT SnesPresetManager
{
public:
    /**
     * @brief Returns a curated collection of built-in presets for classic SNES masterpieces.
     */
    static QList<SnesPreset> builtInPresets();

    /**
     * @brief Loads preset files (.stk or .json) from a directory.
     */
    static QList<SnesPreset> loadFromDirectory(const QString &dirPath);

    /**
     * @brief Parses a single preset file (.stk INI format or .json).
     */
    static SnesPreset loadFromFile(const QString &filePath);

    /**
     * @brief Saves a preset to a JSON or INI file.
     */
    static bool saveToFile(const QString &filePath, const SnesPreset &preset);
};

#endif // SNESPRESET_H
