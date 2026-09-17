#ifndef GODOT_PIPELINE_H
#define GODOT_PIPELINE_H

#include <QString>
#include <QList>
#include <QRect>
#include <QPoint>
#include <QImage>
#include <QMap>
#include "model/spritedocument.h"
#include "cli/cliparser.h"

namespace SpriteStudioCli {

class GodotPipeline
{
public:
    struct ExportArgs {
        QString sheetPath;
        QString tresPath;
        QString scenePath;         ///< Optional .tscn scene generation
        QString explicitUid;       ///< Explicit or requested UID (e.g. uid://...)
        bool preserveExistingUid = true;
        QList<QImage> frames;
        QList<QRect> frameRects;
        QList<QPoint> pivots;
        QMap<QString, SpriteAnimation> animations;
        QImage atlas;
    };

    /**
     * @brief Exports Godot 4 SpriteFrames .tres and optional .tscn scene.
     */
    static CliResult exportGodot(const ExportArgs &args);

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

} // namespace SpriteStudioCli

#endif // GODOT_PIPELINE_H
