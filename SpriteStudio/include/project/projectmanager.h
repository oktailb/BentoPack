#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QString>
#include <QByteArray>
#include <QPointF>
#include "model/spritedocument.h"

/**
 * @brief Handles JSON serialization, deserialization, and workspace file storage
 * for native SpriteStudio projects (.ssp).
 */
class ProjectManager
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
