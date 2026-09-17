#ifndef ASEPRITE_ADAPTER_H
#define ASEPRITE_ADAPTER_H

#include <QString>
#include <QStringList>
#include <QMap>
#include "cli/cliparser.h"
#include "model/spritedocument.h"

namespace SpriteStudioCli {

class AsepriteAdapter
{
public:
    static CliResult execute(const QStringList &args);

    static bool writeAsepriteJson(const QString &jsonPath,
                                  const QString &relImagePath,
                                  const QList<QRect> &packedRects,
                                  const QList<QString> &frameNames,
                                  const QList<QImage> &originalFrames,
                                  const QMap<QString, SpriteAnimation> &animations,
                                  const QSize &atlasSize,
                                  bool jsonArrayFormat,
                                  bool includeTags,
                                  QString *outError = nullptr);
};

} // namespace SpriteStudioCli

#endif // ASEPRITE_ADAPTER_H
