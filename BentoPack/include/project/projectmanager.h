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

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QString>
#include <QByteArray>
#include <QPointF>
#include "bentopackcore_export.h"
#include "model/spritedocument.h"

/**
 * @brief Handles JSON serialization, deserialization, and workspace file storage
 * for native BentoPack projects (.bento).
 */
class BENTOPACK_CORE_EXPORT ProjectManager
{
public:
    /**
     * @brief Serializes a SpriteDocument model and view state to project.json format.
     */
    static QByteArray serializeDocumentToJson(const SpriteDocument &doc,
                                              const QString &relativeAtlasPath = QStringLiteral("assets/atlas.png"),
                                              double zoomFactor = 1.0,
                                              const QPointF &panOffset = QPointF(0, 0));

    /**
     * @brief Deserializes project.json content and loads atlas image into the document.
     */
    static bool deserializeJsonToDocument(const QByteArray &jsonData,
                                          SpriteDocument &outDoc,
                                          const QString &sessionDir,
                                          double *outZoomFactor = nullptr,
                                          QPointF *outPanOffset = nullptr,
                                          QString *errorMsg = nullptr);

    /**
     * @brief Writes atlas image and project.json into the scratch session directory.
     */
    static bool saveProjectToSessionDir(const SpriteDocument &doc,
                                        const QString &sessionDir,
                                        double zoomFactor = 1.0,
                                        const QPointF &panOffset = QPointF(0, 0),
                                        QString *errorMsg = nullptr);

    /**
     * @brief Loads project.json and atlas image from the scratch session directory into outDoc.
     */
    static bool loadProjectFromSessionDir(const QString &sessionDir,
                                          SpriteDocument &outDoc,
                                          double *outZoomFactor = nullptr,
                                          QPointF *outPanOffset = nullptr,
                                          QString *errorMsg = nullptr);
};

#endif // PROJECTMANAGER_H
