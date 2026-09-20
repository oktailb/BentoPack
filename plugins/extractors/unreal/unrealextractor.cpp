// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "unrealextractor.h"
#include "packer/atlaspacker.h"
#include "geometry/triangulator.h"
#include "generated/version.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

UnrealExtractor::UnrealExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool UnrealExtractor::canDecode(const QString &filePath) const
{
    if (filePath.endsWith(QStringLiteral(".paper2d.json"), Qt::CaseInsensitive)) {
        return true;
    }
    if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString head = QString::fromUtf8(file.read(512));
            return head.contains(QStringLiteral("Paper2D_SpriteAtlas")) || head.contains(QStringLiteral("UnrealEngine_Paper2D"));
        }
    }
    return false;
}

bool UnrealExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading Unreal Engine Paper2D atlas %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Failed to open file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            error->code = ExtractorError::ParsingFailed;
            error->message = tr("Failed to parse Unreal JSON: %1").arg(parseErr.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QJsonObject root = doc.object();
    QFileInfo jsonFi(filePath);
    QDir dir = jsonFi.dir();

    // 1. Locate texture atlas
    QString texName = root.value(QStringLiteral("sourceTexture")).toString();
    QString imgPath;
    if (!texName.isEmpty()) {
        if (dir.exists(texName)) {
            imgPath = dir.filePath(texName);
        } else {
            QString fname = QFileInfo(texName).fileName();
            if (dir.exists(fname)) imgPath = dir.filePath(fname);
        }
    }
    if (imgPath.isEmpty() || !QFile::exists(imgPath)) {
        QString base = jsonFi.completeBaseName();
        if (base.endsWith(QStringLiteral(".paper2d"), Qt::CaseInsensitive)) base.chop(8);
        if (QFile::exists(dir.filePath(base + QStringLiteral(".ktx2")))) {
            imgPath = dir.filePath(base + QStringLiteral(".ktx2"));
        } else if (QFile::exists(dir.filePath(base + QStringLiteral(".png")))) {
            imgPath = dir.filePath(base + QStringLiteral(".png"));
        }
    }

    if (imgPath.isEmpty() || !QFile::exists(imgPath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Associated texture atlas image not found for: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(30);

    QString loadErr;
    QImage atlas = VramTextureCompressor::loadAtlasImage(imgPath, &loadErr);
    if (atlas.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to load texture atlas image: %1 (%2)").arg(imgPath, loadErr);
            error->filePath = imgPath;
        }
        return false;
    }

    setProgress(60);

    // 2. Parse sprites array
    QJsonArray spritesArr = root.value(QStringLiteral("sprites")).toArray();
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    QMap<QString, QList<int>> animations;

    for (int i = 0; i < spritesArr.size(); ++i) {
        QJsonObject sObj = spritesArr[i].toObject();
        QString sName = sObj.value(QStringLiteral("name")).toString();
        QJsonObject uvObj = sObj.value(QStringLiteral("sourceUV")).toObject();
        QJsonObject dimObj = sObj.value(QStringLiteral("sourceDimension")).toObject();
        int rx = uvObj.value(QStringLiteral("x")).toInt();
        int ry = uvObj.value(QStringLiteral("y")).toInt();
        int rw = dimObj.value(QStringLiteral("x")).toInt();
        int rh = dimObj.value(QStringLiteral("y")).toInt();

        QRect r(rx, ry, rw, rh);
        r = r.intersected(atlas.rect());
        if (r.isEmpty()) continue;

        QImage frameImg = atlas.copy(r);

        SpriteBox box;
        box.rect = r;
        box.index = frames.size();
        box.selected = false;

        if (sObj.contains(QStringLiteral("pivot")) && sObj[QStringLiteral("pivot")].isObject()) {
            QJsonObject pObj = sObj[QStringLiteral("pivot")].toObject();
            box.pivot = QPoint(pObj.value(QStringLiteral("x")).toInt(), pObj.value(QStringLiteral("y")).toInt());
            box.hasCustomPivot = true;
        }

        if (sObj.contains(QStringLiteral("renderGeometry")) && sObj[QStringLiteral("renderGeometry")].isObject()) {
            QJsonObject renderGeom = sObj[QStringLiteral("renderGeometry")].toObject();
            bool hasTightMesh = renderGeom.value(QStringLiteral("hasTightMesh")).toBool(false);
            if (hasTightMesh) {
                QJsonArray vArr = renderGeom.value(QStringLiteral("vertices")).toArray();
                QJsonArray tArr = renderGeom.value(QStringLiteral("triangles")).toArray();

                QPolygonF poly;
                QList<QPointF> verts;
                for (const QJsonValue &vVal : vArr) {
                    QJsonObject vObj = vVal.toObject();
                    QPointF pt(vObj.value(QStringLiteral("x")).toDouble(), vObj.value(QStringLiteral("y")).toDouble());
                    poly.append(pt);
                    verts.append(pt);
                }

                QList<int> tris;
                for (const QJsonValue &tVal : tArr) {
                    if (tVal.isArray()) {
                        QJsonArray tri = tVal.toArray();
                        for (const QJsonValue &idxVal : tri) {
                            tris.append(idxVal.toInt());
                        }
                    } else {
                        tris.append(tVal.toInt());
                    }
                }

                if (!poly.isEmpty()) {
                    box.hasPolygonMesh = true;
                    box.polygon = poly;
                    box.vertices = verts;
                    box.triangles = tris;
                }
            }
        }

        int fIdx = frames.size();
        frames.append(frameImg);
        boxes.append(box);

        int lastUnderscore = sName.lastIndexOf(QLatin1Char('_'));
        QString animName = (lastUnderscore > 0) ? sName.left(lastUnderscore) : QStringLiteral("default");
        animations[animName].append(fIdx);
    }

    outDoc.clear();
    outDoc.setFilePath(filePath);
    outDoc.setAtlas(atlas);
    outDoc.setFrames(frames, boxes);
    for (auto it = animations.begin(); it != animations.end(); ++it) {
        outDoc.setAnimation(it.key(), it.value(), 12, true);
    }

    setProgress(100);
    setStatusMessage(tr("Imported %1 frames from Unreal Paper2D").arg(frames.size()));
    emit extractionFinished(frames.size());
    return true;
}

bool UnrealExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Packing atlas for Unreal Engine Paper2D..."));
    setProgress(15);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    if (baseName.endsWith(QStringLiteral(".paper2d"), Qt::CaseInsensitive)) {
        baseName.chop(8);
    }
    if (baseName.trimmed().isEmpty()) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Export file name cannot be empty.");
            error->filePath = filePath;
        }
        return false;
    }
    QString imageExt = QStringLiteral(".png");
    if (options.textureFormat == TEXTURE_FORMAT_KTX2_UASTC || options.textureFormat == TEXTURE_FORMAT_KTX2_ETC1S) {
        imageExt = QStringLiteral(".ktx2");
    } else if (options.textureFormat == TEXTURE_FORMAT_BASIS) {
        imageExt = QStringLiteral(".basis");
    }
    QString pngFileName = baseName + imageExt;
    QString pngFilePath = dir.filePath(pngFileName);

    AtlasPacker::PackOptions packOpts = options.packOptions;
    if (options.padding > 0 && packOpts.padding == 2) {
        packOpts.padding = options.padding;
    }

    QList<QPolygonF> docPolygons;
    docPolygons.reserve(doc.frameCount());
    for (int i = 0; i < doc.frameCount(); ++i) {
        docPolygons.append(doc.box(i).hasPolygonMesh ? doc.box(i).polygon : QPolygonF());
    }

    AtlasPackResult packResult;
    if (packOpts.algorithm == AtlasPacker::KeepLayout && !doc.atlas().isNull()) {
        packResult.atlas = doc.atlas();
        packResult.frameRects.reserve(doc.boxes().size());
        for (const SpriteBox &box : doc.boxes()) {
            packResult.frameRects.append(box.rect);
        }
        packResult.dimensions = doc.atlas().size();
        packResult.uniqueFramesCount = doc.frameCount();
        packResult.success = true;
    } else {
        packResult = AtlasPacker::pack(doc.frames(), packOpts, docPolygons);
    }

    if (!packResult.success) {
        if (error) {
            error->code = ExtractorError::PackingFailed;
            error->message = tr("Failed to pack frames for Unreal Paper2D export.");
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(50);

    bool saveOk = false;
    if (options.textureFormat == TEXTURE_FORMAT_KTX2_UASTC || options.textureFormat == TEXTURE_FORMAT_KTX2_ETC1S || options.textureFormat == TEXTURE_FORMAT_BASIS) {
        VramCompressionOptions vOpts = options.vramOptions;
        if (options.textureFormat == TEXTURE_FORMAT_KTX2_UASTC) vOpts.format = VramFormat::KTX2_UASTC;
        else if (options.textureFormat == TEXTURE_FORMAT_KTX2_ETC1S) vOpts.format = VramFormat::KTX2_ETC1S;
        else if (options.textureFormat == TEXTURE_FORMAT_BASIS) vOpts.format = VramFormat::Basis_UASTC;
        QString vErr;
        saveOk = VramTextureCompressor::compressToFile(packResult.atlas, pngFilePath, vOpts, nullptr, &vErr);
        if (!saveOk && error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save companion VRAM texture: %1 (%2)").arg(pngFilePath, vErr);
            error->filePath = pngFilePath;
            return false;
        }
    } else {
        saveOk = packResult.atlas.save(pngFilePath, "PNG");
        if (!saveOk && error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save companion image: %1").arg(pngFilePath);
            error->filePath = pngFilePath;
            return false;
        }
    }

    setProgress(75);

    // Build Unreal Paper2D JSON
    QJsonObject rootObj;
    rootObj["generator"] = QStringLiteral("SpriteStudio");
    rootObj["version"] = QString(PROJECT_VERSION);
    rootObj["type"] = QStringLiteral("Paper2D_SpriteAtlas");
    rootObj["format"] = QStringLiteral("UnrealEngine_Paper2D");
    rootObj["sourceTexture"] = pngFileName;

    QJsonObject texDim;
    texDim["x"] = packResult.dimensions.width();
    texDim["y"] = packResult.dimensions.height();
    rootObj["textureDimension"] = texDim;

    QJsonArray spritesArr;
    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QJsonObject sObj;
        QString sName = QStringLiteral("%1_%2").arg(baseName).arg(i, 4, 10, QLatin1Char('0'));
        sObj["name"] = sName;

        QJsonObject uvObj;
        uvObj["x"] = r.x();
        uvObj["y"] = r.y();
        sObj["sourceUV"] = uvObj;

        QJsonObject dimObj;
        dimObj["x"] = r.width();
        dimObj["y"] = r.height();
        sObj["sourceDimension"] = dimObj;

        QPoint piv = doc.boxPivot(i);
        QJsonObject pivObj;
        pivObj["x"] = piv.x();
        pivObj["y"] = piv.y();
        sObj["pivot"] = pivObj;

        if (i < doc.frameCount()) {
            const SpriteBox &box = doc.box(i);
            bool useMesh = box.hasPolygonMesh && !box.polygon.isEmpty() && !box.triangles.isEmpty();

            QJsonObject renderGeom;
            renderGeom["hasTightMesh"] = useMesh;

            QJsonArray vArr;
            QJsonArray tArr;

            const QList<QPointF> meshVertices = !box.vertices.isEmpty() ? box.vertices :
                (box.polygon.isClosed() && box.polygon.size() >= 4 ? box.polygon.mid(0, box.polygon.size() - 1).toList() : box.polygon.toList());

            if (useMesh) {
                for (const QPointF &pt : meshVertices) {
                    QJsonObject v;
                    v["x"] = pt.x();
                    v["y"] = pt.y();
                    vArr.append(v);
                }
                for (int t = 0; t + 2 < box.triangles.size(); t += 3) {
                    QJsonArray tri;
                    tri.append(box.triangles[t]);
                    tri.append(box.triangles[t + 1]);
                    tri.append(box.triangles[t + 2]);
                    tArr.append(tri);
                }
            } else {
                QList<QPointF> quad = { QPointF(0, 0), QPointF(r.width(), 0), QPointF(r.width(), r.height()), QPointF(0, r.height()) };
                for (const QPointF &pt : quad) {
                    QJsonObject v;
                    v["x"] = pt.x();
                    v["y"] = pt.y();
                    vArr.append(v);
                }
                QJsonArray tri1; tri1.append(0); tri1.append(1); tri1.append(2);
                QJsonArray tri2; tri2.append(0); tri2.append(2); tri2.append(3);
                tArr.append(tri1);
                tArr.append(tri2);
            }
            renderGeom["vertices"] = vArr;
            renderGeom["triangles"] = tArr;
            sObj["renderGeometry"] = renderGeom;

            // Collision geometry
            QJsonObject collisionGeom;
            QJsonArray colPoly;
            if (useMesh) {
                for (const QPointF &pt : meshVertices) {
                    QJsonObject v;
                    v["x"] = pt.x();
                    v["y"] = pt.y();
                    colPoly.append(v);
                }
            }
            collisionGeom["polygon"] = colPoly;
            sObj["collisionGeometry"] = collisionGeom;
        }

        spritesArr.append(sObj);
    }
    rootObj["sprites"] = spritesArr;

    // Flipbooks (Animations)
    QJsonArray flipbooksArr;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        QJsonObject fb;
        fb["name"] = it.key();
        fb["framesPerSecond"] = it.value().fps;
        QJsonArray kf;
        for (int idx : it.value().frameIndices) {
            QString sName = QStringLiteral("%1_%2").arg(baseName).arg(idx, 4, 10, QLatin1Char('0'));
            kf.append(sName);
        }
        fb["keyframes"] = kf;
        flipbooksArr.append(fb);
    }
    rootObj["flipbooks"] = flipbooksArr;

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to Unreal Paper2D JSON file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QJsonDocument jsonDoc(rootObj);
    outFile.write(jsonDoc.toJson(QJsonDocument::Indented));
    outFile.close();

    setProgress(100);
    setStatusMessage(tr("Exported Unreal Paper2D Sprite: %1 and image %2").arg(QFileInfo(filePath).fileName(), pngFileName));
    return true;
}
