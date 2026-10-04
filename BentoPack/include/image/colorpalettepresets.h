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

#ifndef COLORPALETTEPRESETS_H
#define COLORPALETTEPRESETS_H

#include <QVector>
#include <QString>
#include <QColor>
#include <QList>
#include "bentopackcore_export.h"

/**
 * @brief Unified canonical source of truth for retro and studio color palettes,
 * used across the Pixel Editor dialog, RetroPalette filter plugin, and atlas generators.
 */
class BENTOPACK_CORE_EXPORT ColorPalettePresets
{
public:
    enum Preset {
        Standard = 0,      ///< Bento Studio 36-color standard palette
        GameBoyDMG,        ///< Game Boy Original DMG (4 shades of green)
        GameBoyPocket,     ///< Game Boy Pocket / Light (4 true grays)
        NES,               ///< Nintendo Entertainment System / Famicom (54 colors)
        SNES,              ///< Super Nintendo / 16-bit (32 colors)
        Pico8,             ///< PICO-8 Fantasy Console (16 colors)
        Commodore64,       ///< Commodore 64 (16 colors)
        Amiga,             ///< Amiga OCS (32 colors)
        PCEngine,          ///< NEC PC-Engine / TurboGrafx-16 (32 colors)
        CGAMode1,          ///< IBM CGA Mode 1 (Black, Cyan, Magenta, White)
        CGAMode2,          ///< IBM CGA Mode 2 (Black, Green, Red, Yellow)
        Endesga32,         ///< EDG 32 by Endesga (32 curated colors)
        Custom             ///< User-defined imported palette
    };

    /**
     * @brief Returns the list of color values (QRgb) for a given preset.
     */
    static QVector<QRgb> getPresetPalette(Preset preset);

    /**
     * @brief Returns a localized/human-readable display name for the preset.
     */
    static QString getPresetName(Preset preset);

    /**
     * @brief Returns all available built-in presets (excluding Custom).
     */
    static QList<Preset> allPresets();

    /**
     * @brief Parses and extracts palette colors from a file (.hex, .gpl, .pal, or .png/.bmp image).
     */
    static QVector<QRgb> loadPaletteFromFile(const QString &filePath, QString *outError = nullptr);
};

#endif // COLORPALETTEPRESETS_H
