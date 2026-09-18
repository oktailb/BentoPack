#include "include/project/projectmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QImage>

QByteArray ProjectManager::serializeDocumentToJson(const SpriteDocument &doc,
                                                    const QString &relativeAtlasPath,
                                                    double zoomFactor,
                                                    const QPointF &panOffset)
{
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("SpriteStudioProject");
    root[QStringLiteral("version")] = QStringLiteral("1.0");
    root[QStringLiteral("generator")] = QStringLiteral("SpriteStudio");
    root[QStringLiteral("name")] = doc.projectName().isEmpty() ? QStringLiteral("New Project") : doc.projectName();
    root[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    // Atlas information
    QJsonObject atlasObj;
    atlasObj[QStringLiteral("file")] = relativeAtlasPath;
    atlasObj[QStringLiteral("width")] = doc.atlas().width();
    atlasObj[QStringLiteral("height")] = doc.atlas().height();
    root[QStringLiteral("atlas")] = atlasObj;

    // View state
    QJsonObject viewObj;
    viewObj[QStringLiteral("zoomFactor")] = zoomFactor;
    viewObj[QStringLiteral("panX")] = panOffset.x();
    viewObj[QStringLiteral("panY")] = panOffset.y();
    root[QStringLiteral("viewState")] = viewObj;

    // Slices / Bounding boxes
    QJsonArray boxesArray;
    const QList<SpriteBox> &boxes = doc.boxes();
    for (int i = 0; i < boxes.size(); ++i) {
        const SpriteBox &b = boxes.at(i);
        QJsonObject bObj;
        bObj[QStringLiteral("index")] = b.index;
        bObj[QStringLiteral("selected")] = b.selected;
        bObj[QStringLiteral("groupId")] = b.groupId;

        QJsonObject rObj;
        rObj[QStringLiteral("x")] = b.rect.x();
        rObj[QStringLiteral("y")] = b.rect.y();
        rObj[QStringLiteral("w")] = b.rect.width();
        rObj[QStringLiteral("h")] = b.rect.height();
        bObj[QStringLiteral("rect")] = rObj;

        QJsonObject pObj;
        pObj[QStringLiteral("x")] = b.effectivePivot().x();
        pObj[QStringLiteral("y")] = b.effectivePivot().y();
        pObj[QStringLiteral("custom")] = b.hasCustomPivot;
        bObj[QStringLiteral("pivot")] = pObj;

        if (!b.overlappingBoxes.isEmpty()) {
            QJsonArray ovArray;
            for (int ov : b.overlappingBoxes) {
                ovArray.append(ov);
            }
            bObj[QStringLiteral("overlapping")] = ovArray;
        }

        if (b.hasPolygonMesh) {
            bObj[QStringLiteral("hasPolygonMesh")] = true;
            QJsonArray polyArray;
            for (const QPointF &pt : b.polygon) {
                polyArray.append(pt.x());
                polyArray.append(pt.y());
            }
            bObj[QStringLiteral("polygon")] = polyArray;

            QJsonArray vertArray;
            for (const QPointF &pt : b.vertices) {
                vertArray.append(pt.x());
                vertArray.append(pt.y());
            }
            bObj[QStringLiteral("vertices")] = vertArray;

            QJsonArray triArray;
            for (int t : b.triangles) {
                triArray.append(t);
            }
            bObj[QStringLiteral("triangles")] = triArray;
        }

        boxesArray.append(bObj);
    }
    root[QStringLiteral("boxes")] = boxesArray;

    // Animations
    QJsonArray animsArray;
    const auto &anims = doc.animations();
    for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
        const SpriteAnimation &anim = it.value();
        QJsonObject aObj;
        aObj[QStringLiteral("name")] = anim.name;
        aObj[QStringLiteral("fps")] = anim.fps;
        aObj[QStringLiteral("loop")] = anim.loop;
        QString loopModeStr;
        switch (anim.loopMode) {
            case SpriteAnimation::Once: loopModeStr = QStringLiteral("once"); break;
            case SpriteAnimation::PingPong: loopModeStr = QStringLiteral("pingpong"); break;
            case SpriteAnimation::Loop:
            default: loopModeStr = QStringLiteral("loop"); break;
        }
        aObj[QStringLiteral("loop_mode")] = loopModeStr;

        QJsonArray fArray;
        for (int frameIdx : anim.frameIndices) {
            fArray.append(frameIdx);
        }
        aObj[QStringLiteral("frames")] = fArray;

        animsArray.append(aObj);
    }
    root[QStringLiteral("animations")] = animsArray;

    QJsonDocument jsonDoc(root);
    return jsonDoc.toJson(QJsonDocument::Indented);
}

bool ProjectManager::deserializeJsonToDocument(const QByteArray &jsonData,
                                              SpriteDocument &outDoc,
                                              const QString &sessionDir,
                                              double *outZoomFactor,
                                              QPointF *outPanOffset,
                                              QString *errorMsg)
{
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("JSON Parse Error: ") + parseError.errorString();
        return false;
    }

    QJsonObject root = jsonDoc.object();

    // Verify format signature
    QString format = root.value(QStringLiteral("format")).toString();
    if (format != QStringLiteral("SpriteStudioProject")) {
        // Fallback: accept if it contains "boxes" and "atlas"
        if (!root.contains(QStringLiteral("boxes")) && !root.contains(QStringLiteral("atlas"))) {
            if (errorMsg) *errorMsg = QStringLiteral("Invalid project format signature.");
            return false;
        }
    }

    // View state
    if (root.contains(QStringLiteral("viewState"))) {
        QJsonObject viewObj = root.value(QStringLiteral("viewState")).toObject();
        if (outZoomFactor) {
            *outZoomFactor = viewObj.value(QStringLiteral("zoomFactor")).toDouble(1.0);
        }
        if (outPanOffset) {
            *outPanOffset = QPointF(viewObj.value(QStringLiteral("panX")).toDouble(0.0),
                                   viewObj.value(QStringLiteral("panY")).toDouble(0.0));
        }
    }

    // Clear document
    outDoc.clear();

    // Load Atlas
    QJsonObject atlasObj = root.value(QStringLiteral("atlas")).toObject();
    QString relativeAtlasFile = atlasObj.value(QStringLiteral("file")).toString(QStringLiteral("assets/atlas.png"));
    QImage atlasImage;

    if (!relativeAtlasFile.isEmpty()) {
        QString fullAtlasPath = QDir(sessionDir).filePath(relativeAtlasFile);
        if (QFile::exists(fullAtlasPath)) {
            atlasImage.load(fullAtlasPath);
            if (!atlasImage.isNull()) {
                outDoc.setAtlas(atlasImage);
            }
        }
    }

    // Reconstruct Boxes and Frames
    QJsonArray boxesArray = root.value(QStringLiteral("boxes")).toArray();
    QList<SpriteBox> boxes;
    QList<QImage> frames;
    boxes.reserve(boxesArray.size());
    frames.reserve(boxesArray.size());

    for (int i = 0; i < boxesArray.size(); ++i) {
        QJsonObject bObj = boxesArray.at(i).toObject();
        QJsonObject rObj = bObj.value(QStringLiteral("rect")).toObject();

        QRect rect(
            rObj.value(QStringLiteral("x")).toInt(0),
            rObj.value(QStringLiteral("y")).toInt(0),
            rObj.value(QStringLiteral("w")).toInt(0),
            rObj.value(QStringLiteral("h")).toInt(0)
        );

        SpriteBox box;
        box.rect = rect;
        box.index = bObj.value(QStringLiteral("index")).toInt(i);
        box.selected = bObj.value(QStringLiteral("selected")).toBool(false);
        box.groupId = bObj.value(QStringLiteral("groupId")).toInt(0);

        if (bObj.contains(QStringLiteral("overlapping"))) {
            QJsonArray ovArray = bObj.value(QStringLiteral("overlapping")).toArray();
            for (const QJsonValue &v : ovArray) {
                box.overlappingBoxes.append(v.toInt());
            }
        }

        if (bObj.contains(QStringLiteral("pivot"))) {
            QJsonObject pObj = bObj.value(QStringLiteral("pivot")).toObject();
            box.pivot = QPoint(pObj.value(QStringLiteral("x")).toInt(rect.width() / 2),
                               pObj.value(QStringLiteral("y")).toInt(rect.height()));
            box.hasCustomPivot = pObj.value(QStringLiteral("custom")).toBool(false);
        } else {
            box.pivot = QPoint(rect.width() / 2, rect.height());
            box.hasCustomPivot = false;
        }

        if (bObj.value(QStringLiteral("hasPolygonMesh")).toBool(false)) {
            box.hasPolygonMesh = true;
            QJsonArray polyArray = bObj.value(QStringLiteral("polygon")).toArray();
            for (int pIdx = 0; pIdx + 1 < polyArray.size(); pIdx += 2) {
                box.polygon.append(QPointF(polyArray[pIdx].toDouble(), polyArray[pIdx + 1].toDouble()));
            }
            if (bObj.contains(QStringLiteral("vertices"))) {
                QJsonArray vertArray = bObj.value(QStringLiteral("vertices")).toArray();
                for (int vIdx = 0; vIdx + 1 < vertArray.size(); vIdx += 2) {
                    box.vertices.append(QPointF(vertArray[vIdx].toDouble(), vertArray[vIdx + 1].toDouble()));
                }
            } else {
                box.vertices = box.polygon.toList();
                if (box.vertices.size() >= 4 && box.vertices.first() == box.vertices.last()) {
                    box.vertices.removeLast();
                }
            }

            QJsonArray triArray = bObj.value(QStringLiteral("triangles")).toArray();
            for (const QJsonValue &v : triArray) {
                box.triangles.append(v.toInt());
            }
        }

        boxes.append(box);

        // Crop frame from atlas if available
        if (!atlasImage.isNull() && rect.isValid()) {
            QRect intersect = rect.intersected(atlasImage.rect());
            if (intersect.isValid() && !intersect.isEmpty()) {
                frames.append(atlasImage.copy(intersect));
            } else {
                frames.append(QImage());
            }
        } else {
            frames.append(QImage());
        }
    }

    outDoc.setFrames(frames, boxes);

    // Reconstruct Animations
    QJsonArray animsArray = root.value(QStringLiteral("animations")).toArray();
    for (int i = 0; i < animsArray.size(); ++i) {
        QJsonObject aObj = animsArray.at(i).toObject();
        QString name = aObj.value(QStringLiteral("name")).toString();
        int fps = aObj.value(QStringLiteral("fps")).toInt(12);
        bool loop = aObj.value(QStringLiteral("loop")).toBool(true);
        QString loopModeStr = aObj.value(QStringLiteral("loop_mode")).toString();
        SpriteAnimation::LoopMode loopMode = SpriteAnimation::Loop;
        if (loopModeStr == QLatin1String("once")) {
            loopMode = SpriteAnimation::Once;
        } else if (loopModeStr == QLatin1String("pingpong")) {
            loopMode = SpriteAnimation::PingPong;
        } else if (!loopModeStr.isEmpty()) {
            loopMode = SpriteAnimation::Loop;
        } else {
            loopMode = loop ? SpriteAnimation::Loop : SpriteAnimation::Once;
        }

        QList<int> frameIndices;
        QJsonArray fArray = aObj.value(QStringLiteral("frames")).toArray();
        for (const QJsonValue &v : fArray) {
            frameIndices.append(v.toInt());
        }

        if (!name.isEmpty()) {
            outDoc.setAnimation(name, frameIndices, fps, (loopMode == SpriteAnimation::Loop), loopMode);
        }
    }

    return true;
}

bool ProjectManager::saveProjectToSessionDir(const SpriteDocument &doc,
                                            const QString &sessionDir,
                                            double zoomFactor,
                                            const QPointF &panOffset,
                                            QString *errorMsg)
{
    if (sessionDir.isEmpty()) {
        if (errorMsg) *errorMsg = QStringLiteral("Session directory path is empty.");
        return false;
    }

    QDir sDir(sessionDir);
    if (!sDir.exists()) {
        sDir.mkpath(sessionDir);
    }

    QString assetsDir = sDir.filePath(QStringLiteral("assets"));
    QDir().mkpath(assetsDir);

    // Save Atlas Image if present
    QString relativeAtlasPath;
    if (!doc.atlas().isNull()) {
        relativeAtlasPath = QStringLiteral("assets/atlas.png");
        QString atlasFullPath = sDir.filePath(relativeAtlasPath);
        if (!doc.atlas().save(atlasFullPath, "PNG")) {
            if (errorMsg) *errorMsg = QStringLiteral("Failed to save atlas image to ") + atlasFullPath;
            return false;
        }
    }

    // Serialize JSON
    QByteArray jsonData = serializeDocumentToJson(doc, relativeAtlasPath, zoomFactor, panOffset);
    QString projectJsonPath = sDir.filePath(QStringLiteral("project.json"));
    QFile file(projectJsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMsg) *errorMsg = QStringLiteral("Failed to write project.json to ") + projectJsonPath;
        return false;
    }

    file.write(jsonData);
    file.close();
    return true;
}

bool ProjectManager::loadProjectFromSessionDir(const QString &sessionDir,
                                              SpriteDocument &outDoc,
                                              double *outZoomFactor,
                                              QPointF *outPanOffset,
                                              QString *errorMsg)
{
    if (sessionDir.isEmpty() || !QDir(sessionDir).exists()) {
        if (errorMsg) *errorMsg = QStringLiteral("Session directory does not exist: ") + sessionDir;
        return false;
    }

    QString projectJsonPath = QDir(sessionDir).filePath(QStringLiteral("project.json"));
    QFile file(projectJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg) *errorMsg = QStringLiteral("Cannot open project.json in ") + sessionDir;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    return deserializeJsonToDocument(jsonData, outDoc, sessionDir, outZoomFactor, outPanOffset, errorMsg);
}
