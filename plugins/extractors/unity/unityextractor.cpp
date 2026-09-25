// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "unityextractor.h"
#include "license/licensemanager.h"
#include "packer/atlaspacker.h"
#include "geometry/triangulator.h"
#include "generated/version.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

UnityExtractor::UnityExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool UnityExtractor::canDecode(const QString &filePath) const
{
    if (filePath.endsWith(QStringLiteral(".unity.json"), Qt::CaseInsensitive)) {
        return true;
    }
    if (filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString head = QString::fromUtf8(file.read(512));
            return head.contains(QStringLiteral("Unity2D_SpriteMesh"));
        }
    }
    return false;
}

bool UnityExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading Unity 2D Sprite Mesh %1...").arg(QFileInfo(filePath).fileName()));
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
            error->message = tr("Failed to parse Unity JSON: %1").arg(parseErr.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QJsonObject root = doc.object();
    QFileInfo jsonFi(filePath);
    QDir dir = jsonFi.dir();

    // 1. Locate texture atlas
    QString texName = root.value(QStringLiteral("texture")).toString();
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
        if (base.endsWith(QStringLiteral(".unity"), Qt::CaseInsensitive)) base.chop(6);
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
        QJsonObject rectObj = sObj.value(QStringLiteral("rect")).toObject();
        int rx = rectObj.value(QStringLiteral("x")).toInt();
        int ry = rectObj.value(QStringLiteral("y")).toInt();
        int rw = rectObj.value(QStringLiteral("w")).toInt();
        int rh = rectObj.value(QStringLiteral("h")).toInt();

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
            double px = pObj.value(QStringLiteral("x")).toDouble(0.5);
            double py = pObj.value(QStringLiteral("y")).toDouble(1.0);
            box.pivot = QPoint(qRound(px * r.width()), qRound(py * r.height()));
            box.hasCustomPivot = true;
        }

        bool hasTightMesh = sObj.value(QStringLiteral("hasTightMesh")).toBool(false);
        QJsonArray vArr = sObj.value(QStringLiteral("vertices")).toArray();
        QJsonArray tArr = sObj.value(QStringLiteral("triangles")).toArray();

        if (hasTightMesh && !vArr.isEmpty()) {
            QPolygonF poly;
            QList<QPointF> verts;
            for (const QJsonValue &vVal : vArr) {
                if (vVal.isArray()) {
                    QJsonArray xy = vVal.toArray();
                    if (xy.size() >= 2) {
                        QPointF pt(xy[0].toDouble(), xy[1].toDouble());
                        poly.append(pt);
                        verts.append(pt);
                    }
                }
            }
            QList<int> tris;
            for (const QJsonValue &tVal : tArr) {
                tris.append(tVal.toInt());
            }
            if (!poly.isEmpty()) {
                box.hasPolygonMesh = true;
                box.polygon = poly;
                box.vertices = verts;
                box.triangles = tris;
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
    setStatusMessage(tr("Imported %1 frames from Unity 2D Sprite Mesh").arg(frames.size()));
    emit extractionFinished(frames.size());
    return true;
}

bool UnityExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Packing atlas for Unity..."));
    setProgress(15);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    if (baseName.endsWith(QStringLiteral(".unity"), Qt::CaseInsensitive)) {
        baseName.chop(6);
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
            error->message = tr("Failed to pack frames for Unity export.");
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(50);

    BentoPack::LicenseManager::applyWatermark(packResult.atlas);

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

    // Build Unity JSON descriptor
    QJsonObject rootObj;
    rootObj["generator"] = BentoPack::LicenseManager::isCommercial() ? QStringLiteral("BentoPack") : QStringLiteral("BentoPack Community Edition");
    rootObj["version"] = QString(PROJECT_VERSION);
    rootObj["format"] = QStringLiteral("Unity2D_SpriteMesh");
    rootObj["texture"] = pngFileName;
    BentoPack::LicenseManager::applyWatermark(rootObj);

    QJsonObject texSize;
    texSize["w"] = packResult.dimensions.width();
    texSize["h"] = packResult.dimensions.height();
    rootObj["textureSize"] = texSize;

    const double aW = packResult.dimensions.width();
    const double aH = packResult.dimensions.height();

    QJsonArray spritesArr;
    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QJsonObject sObj;
        QString sName = QStringLiteral("%1_%2").arg(baseName).arg(i, 4, 10, QLatin1Char('0'));
        sObj["name"] = sName;

        QJsonObject rectObj;
        rectObj["x"] = r.x();
        rectObj["y"] = r.y();
        rectObj["w"] = r.width();
        rectObj["h"] = r.height();
        sObj["rect"] = rectObj;

        QPoint piv = doc.boxPivot(i);
        double normX = r.width() > 0 ? static_cast<double>(piv.x()) / r.width() : 0.5;
        double normY = r.height() > 0 ? static_cast<double>(piv.y()) / r.height() : 1.0;
        QJsonObject pivotObj;
        pivotObj["x"] = normX;
        pivotObj["y"] = normY;
        sObj["pivot"] = pivotObj;

        if (i < doc.frameCount()) {
            const SpriteBox &box = doc.box(i);
            bool useMesh = box.hasPolygonMesh && !box.polygon.isEmpty() && !box.triangles.isEmpty();
            sObj["hasTightMesh"] = useMesh;

            QJsonArray vArr;
            QJsonArray uvArr;
            QJsonArray tArr;

            if (useMesh) {
                const QList<QPointF> meshVertices = !box.vertices.isEmpty() ? box.vertices :
                    (box.polygon.isClosed() && box.polygon.size() >= 4 ? box.polygon.mid(0, box.polygon.size() - 1).toList() : box.polygon.toList());

                for (const QPointF &pt : meshVertices) {
                    QJsonArray xy;
                    xy.append(pt.x());
                    xy.append(pt.y());
                    vArr.append(xy);

                    QJsonArray uv;
                    double u = (aW > 0.0) ? ((r.x() + pt.x()) / aW) : 0.0;
                    double v = (aH > 0.0) ? (1.0 - (r.y() + pt.y()) / aH) : 0.0; // Unity UV Y is inverted
                    uv.append(u);
                    uv.append(v);
                    uvArr.append(uv);
                }
                for (int tIdx : box.triangles) {
                    tArr.append(tIdx);
                }
            } else {
                // Fallback quad mesh
                QList<QPointF> quad = { QPointF(0, 0), QPointF(r.width(), 0), QPointF(r.width(), r.height()), QPointF(0, r.height()) };
                for (const QPointF &pt : quad) {
                    QJsonArray xy;
                    xy.append(pt.x());
                    xy.append(pt.y());
                    vArr.append(xy);

                    QJsonArray uv;
                    double u = (aW > 0.0) ? ((r.x() + pt.x()) / aW) : 0.0;
                    double v = (aH > 0.0) ? (1.0 - (r.y() + pt.y()) / aH) : 0.0;
                    uv.append(u);
                    uv.append(v);
                    uvArr.append(uv);
                }
                tArr.append(0); tArr.append(1); tArr.append(2);
                tArr.append(0); tArr.append(2); tArr.append(3);
            }
            sObj["vertices"] = vArr;
            sObj["uvs"] = uvArr;
            sObj["triangles"] = tArr;
        }

        spritesArr.append(sObj);
    }
    rootObj["sprites"] = spritesArr;

    // Animations map
    QJsonObject animsObj;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        QJsonObject a;
        a["fps"] = it.value().fps;
        a["loop"] = it.value().loop;
        QJsonArray fIndices;
        for (int idx : it.value().frameIndices) {
            fIndices.append(idx);
        }
        a["frames"] = fIndices;
        animsObj[it.key()] = a;
    }
    rootObj["animations"] = animsObj;

    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to Unity JSON file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QJsonDocument jsonDoc(rootObj);
    outFile.write(jsonDoc.toJson(QJsonDocument::Indented));
    outFile.close();

    setProgress(100);
    setStatusMessage(tr("Exported Unity Sprite Mesh: %1 and image %2").arg(QFileInfo(filePath).fileName(), pngFileName));
    return true;
}
