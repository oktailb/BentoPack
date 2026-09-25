// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef GODOT_PIPELINE_H
#define GODOT_PIPELINE_H

#include <QString>
#include <QList>
#include <QRect>
#include <QPoint>
#include <QImage>
#include <QMap>
#include "model/spritedocument.h"

class GodotPipeline
{
public:
    /**
     * @brief Extracts existing UID from a .tres file if present.
     */
    static QString extractExistingUid(const QString &tresPath);

    /**
     * @brief Generates a deterministic Godot 4 style UID (e.g. uid://...)
     */
    static QString generateDeterministicUid(const QString &seed);

    /**
     * @brief Generates a standard Godot 4 AnimatedSprite2D scene (.tscn).
     */
    static bool generateScene(const QString &scenePath,
                              const QString &tresPath,
                              const QString &defaultAnimName,
                              QString *outError = nullptr);
};

#endif // GODOT_PIPELINE_H
