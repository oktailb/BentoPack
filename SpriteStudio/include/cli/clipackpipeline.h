/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

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
