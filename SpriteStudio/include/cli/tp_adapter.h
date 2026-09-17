#ifndef TP_ADAPTER_H
#define TP_ADAPTER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QImage>
#include <QPoint>
#include <QMap>
#include "cli/cliparser.h"
#include "packer/atlaspacker.h"
#include "model/spritedocument.h"

namespace SpriteStudioCli {

class TexturePackerAdapter
{
public:
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

    /**
     * @brief Generates TexturePacker compliant JSON data (Array or Hash format).
     */
    static bool writeTexturePackerJson(const QString &jsonPath,
                                       const QString &relImagePath,
                                       const QList<QRect> &packedRects,
                                       const QList<QString> &frameNames,
                                       const QList<QImage> &originalFrames,
                                       const QList<QPoint> &pivots,
                                       const QSize &atlasSize,
                                       bool jsonArrayFormat,
                                       QString *outError = nullptr);
};

} // namespace SpriteStudioCli

#endif // TP_ADAPTER_H
