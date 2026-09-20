#ifndef CLIPACKPIPELINE_H
#define CLIPACKPIPELINE_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QImage>
#include <QPoint>
#include <QMap>
#include "spritestudiocore_export.h"
#include "cli/cliparser.h"
#include "packer/atlaspacker.h"
#include "model/spritedocument.h"

namespace SpriteStudioCli {

class SPRITESTUDIO_CORE_EXPORT CliPackPipeline
{
public:
    /**
     * @brief Executes generic atlas packing and delegates metadata export dynamically to Extractor plugins.
     */
    static CliResult execute(const QStringList &args);

    /**
     * @brief Recursively or flatly gathers input images from files or directories.
     */
    static bool collectInputImages(const QStringList &inputPaths,
                                   QList<QImage> &outFrames,
                                   QList<QString> &outNames,
                                   QList<QPoint> &outPivots,
                                   QMap<QString, SpriteAnimation> &outAnimations,
                                   bool prependFolderName = false,
                                   QString *outError = nullptr);
};

} // namespace SpriteStudioCli

#endif // CLIPACKPIPELINE_H
