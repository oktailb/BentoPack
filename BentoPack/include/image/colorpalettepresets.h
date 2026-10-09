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
        Standard = 0       ///< Bento Studio 36-color standard palette (built-in fallback)
    };

    struct PaletteInfo {
        QString id;              ///< Unique identifier / base filename (e.g. "nes", "pico8", "bento_standard")
        QString name;            ///< Display name extracted from file header (or filename)
        QString filePath;        ///< File path or resource path (e.g. ":/palettes/nes.gpl")
        QVector<QRgb> colors;    ///< Loaded colors
        bool isBuiltIn = false;  ///< True if embedded Qt resource
    };

    /**
     * @brief Returns the built-in hardcoded 36-color Bento Studio standard palette (emergency fallback).
     */
    static QVector<QRgb> standardPalette();

    /**
     * @brief Returns palette colors by unique identifier (e.g. "bento_standard", "nes", "gameboy_dmg").
     */
    static QVector<QRgb> getPaletteById(const QString &id);

    /**
     * @brief Returns palette colors by index in availablePalettes().
     */
    static QVector<QRgb> getPaletteByIndex(int index);

    /**
     * @brief Returns the standard fallback preset palette.
     */
    static QVector<QRgb> getPresetPalette(Preset preset = Standard);

    /**
     * @brief Returns all scanned and available palettes (built-ins + user palettes found on disk).
     */
    static QList<PaletteInfo> availablePalettes();

    /**
     * @brief Scans directories (application, user config, and Qt resources) for .gpl, .hex, and .pal files.
     */
    static void scanPalettes();

    /**
     * @brief Returns the search directories used for user and application palettes.
     */
    static QStringList paletteSearchPaths();

    /**
     * @brief Ensures the user custom palettes folder exists on disk, and returns its path.
     */
    static QString userPalettesDirectory();

    /**
     * @brief Parses and extracts palette colors from a file (.hex, .gpl, .pal, or .png/.bmp image).
     * @param outName Optional pointer to receive extracted palette name (e.g. from GIMP header).
     */
    static QVector<QRgb> loadPaletteFromFile(const QString &filePath, QString *outError = nullptr, QString *outName = nullptr);
};

#endif // COLORPALETTEPRESETS_H
