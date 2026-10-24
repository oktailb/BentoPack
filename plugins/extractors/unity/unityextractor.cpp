/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "unityextractor.h"
#include "packer/atlaspacker.h"
#include "packer/multiatlaspacker.h"
#include "packer/multipagepacker.h"
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
            QList<QPointF> verts;
            for (const QJsonValue &vVal : vArr) {
                if (vVal.isArray()) {
                    QJsonArray xy = vVal.toArray();
                    if (xy.size() >= 2) {
                        QPointF pt(xy[0].toDouble(), xy[1].toDouble());
                        verts.append(pt);
                    }
                }
            }
            QList<int> tris;
            for (const QJsonValue &tVal : tArr) {
                tris.append(tVal.toInt());
            }

            QPolygonF poly;
            if (sObj.contains(QStringLiteral("polygon"))) {
                QJsonArray pArr = sObj.value(QStringLiteral("polygon")).toArray();
                for (const QJsonValue &pVal : pArr) {
                    if (pVal.isArray()) {
                        QJsonArray xy = pVal.toArray();
                        if (xy.size() >= 2) {
                            poly.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
                        }
                    }
                }
            }
            if (poly.isEmpty() && !verts.isEmpty()) {
                poly = BentoPackGeometry::Triangulator::extractBoundaryContour(verts, tris);
            }

            if (!verts.isEmpty()) {
                box.hasPolygonMesh = true;
                box.polygon = poly;
                box.vertices = verts;
                box.triangles = tris;
            }
        }

        if (sObj.contains(QStringLiteral("weights")) && sObj[QStringLiteral("weights")].isArray()) {
            QJsonArray wArr = sObj.value(QStringLiteral("weights")).toArray();
            QList<float> weights;
            weights.reserve(wArr.size());
            for (const QJsonValue &wv : wArr) {
                weights.append(static_cast<float>(wv.toDouble()));
            }
            box.vertexWeights = weights;
        }

        if (sObj.contains(QStringLiteral("sockets")) && sObj[QStringLiteral("sockets")].isObject()) {
            QJsonObject sockObj = sObj.value(QStringLiteral("sockets")).toObject();
            QMap<QString, QPointF> socks;
            for (auto sit = sockObj.begin(); sit != sockObj.end(); ++sit) {
                if (sit.value().isObject()) {
                    QJsonObject o = sit.value().toObject();
                    socks.insert(sit.key(), QPointF(o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble()));
                } else if (sit.value().isArray()) {
                    QJsonArray a = sit.value().toArray();
                    if (a.size() >= 2) {
                        socks.insert(sit.key(), QPointF(a[0].toDouble(), a[1].toDouble()));
                    }
                }
            }
            box.sockets = socks;
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
        docPolygons.append(doc.box(i).polygon.size() >= 3 ? doc.box(i).polygon : QPolygonF());
    }

    AtlasPackResult packResult;
    MultiAtlasPackResult multiResult;
    MultiPagePackResult mpResult;
    bool isMultiPage = false;
    bool hasAux = options.exportMaterialMaps && doc.hasAnyAuxiliaryMaps();

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
        mpResult = MultiPagePacker::pack(doc, packOpts, hasAux, options.normalMapYFlip);
        if (!mpResult.success) {
            if (error) {
                error->code = ExtractorError::PackingFailed;
                error->message = tr("Failed to pack frames for Unity export.");
                error->filePath = filePath;
            }
            return false;
        }

        if (mpResult.isMultiPage()) {
            isMultiPage = true;
        } else {
            packResult.atlas = mpResult.pages[0].atlas;
            packResult.dimensions = mpResult.pages[0].dimensions;
            packResult.frameRects = mpResult.frameRects;
            packResult.efficiency = mpResult.pages[0].efficiency;
            packResult.uniqueFramesCount = doc.frameCount();
            packResult.success = true;
            if (hasAux) {
                multiResult.master = packResult;
                multiResult.maps = mpResult.pages[0].auxiliaryAtlases;
                multiResult.auxiliaryAtlases = mpResult.pages[0].auxiliaryAtlases;
                multiResult.success = true;
            }
        }
    }

    setProgress(50);

    QList<QString> pageFilenames;
    QList<QString> pageNormalFilenames;
    QList<QString> pageSpecFilenames;
    QList<QString> pageEmissiveFilenames;

    if (isMultiPage) {
        for (int p = 0; p < mpResult.totalPages(); ++p) {
            const AtlasPage &page = mpResult.pages[p];
            QString pageImgFilename = QStringLiteral("%1_%2%3").arg(baseName).arg(p).arg(imageExt);
            QString pageImgPath = dir.filePath(pageImgFilename);
            pageFilenames.append(pageImgFilename);

            bool saveOk = false;
            if (options.textureFormat == TEXTURE_FORMAT_KTX2_UASTC || options.textureFormat == TEXTURE_FORMAT_KTX2_ETC1S || options.textureFormat == TEXTURE_FORMAT_BASIS) {
                VramCompressionOptions vOpts = options.vramOptions;
                if (options.textureFormat == TEXTURE_FORMAT_KTX2_UASTC) vOpts.format = VramFormat::KTX2_UASTC;
                else if (options.textureFormat == TEXTURE_FORMAT_KTX2_ETC1S) vOpts.format = VramFormat::KTX2_ETC1S;
                else if (options.textureFormat == TEXTURE_FORMAT_BASIS) vOpts.format = VramFormat::Basis_UASTC;
                QString vErr;
                saveOk = VramTextureCompressor::compressToFile(page.atlas, pageImgPath, vOpts, nullptr, &vErr);
            } else {
                saveOk = page.atlas.save(pageImgPath, "PNG");
            }
            if (!saveOk && error) {
                error->code = ExtractorError::WriteFailed;
                error->message = tr("Failed to save companion image: %1").arg(pageImgPath);
                error->filePath = pageImgPath;
                return false;
            }

            if (hasAux) {
                if (page.hasAuxiliaryMap(MaterialMapType::Normal)) {
                    QString nfn = QStringLiteral("%1_%2_n%3").arg(baseName).arg(p).arg(imageExt);
                    page.auxiliaryAtlases.value(MaterialMapType::Normal).save(dir.filePath(nfn), "PNG");
                    pageNormalFilenames.append(nfn);
                } else {
                    pageNormalFilenames.append(QString());
                }
                if (page.hasAuxiliaryMap(MaterialMapType::Specular)) {
                    QString sfn = QStringLiteral("%1_%2_s%3").arg(baseName).arg(p).arg(imageExt);
                    page.auxiliaryAtlases.value(MaterialMapType::Specular).save(dir.filePath(sfn), "PNG");
                    pageSpecFilenames.append(sfn);
                } else {
                    pageSpecFilenames.append(QString());
                }
                if (page.hasAuxiliaryMap(MaterialMapType::Emissive)) {
                    QString efn = QStringLiteral("%1_%2_e%3").arg(baseName).arg(p).arg(imageExt);
                    page.auxiliaryAtlases.value(MaterialMapType::Emissive).save(dir.filePath(efn), "PNG");
                    pageEmissiveFilenames.append(efn);
                } else {
                    pageEmissiveFilenames.append(QString());
                }
            }
        }
    } else {
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
    }

    // Save synchronized auxiliary material maps if present (single-page mode)
    QString normalFileName, specFileName, emissiveFileName;
    if (!isMultiPage && hasAux) {
        if (multiResult.hasMap(MaterialMapType::Normal)) {
            normalFileName = baseName + QStringLiteral("_n") + imageExt;
            multiResult.maps[MaterialMapType::Normal].save(dir.filePath(normalFileName), "PNG");
        }
        if (multiResult.hasMap(MaterialMapType::Specular)) {
            specFileName = baseName + QStringLiteral("_s") + imageExt;
            multiResult.maps[MaterialMapType::Specular].save(dir.filePath(specFileName), "PNG");
        }
        if (multiResult.hasMap(MaterialMapType::Emissive)) {
            emissiveFileName = baseName + QStringLiteral("_e") + imageExt;
            multiResult.maps[MaterialMapType::Emissive].save(dir.filePath(emissiveFileName), "PNG");
        }
    }

    setProgress(75);

    // Build Unity JSON descriptor
    QJsonObject rootObj;
    rootObj["generator"] = QStringLiteral("BentoPack");
    rootObj["version"] = QString(PROJECT_VERSION);
    rootObj["format"] = QStringLiteral("Unity2D_SpriteMesh");

    if (isMultiPage) {
        rootObj["isMultiPage"] = true;
        rootObj["pagesCount"] = mpResult.totalPages();
        rootObj["texture"] = pageFilenames.first(); // Primary / fallback texture
        QJsonArray pagesArr;
        for (int p = 0; p < mpResult.totalPages(); ++p) {
            QJsonObject pObj;
            pObj["index"] = p;
            pObj["texture"] = pageFilenames[p];
            pObj["w"] = mpResult.pages[p].dimensions.width();
            pObj["h"] = mpResult.pages[p].dimensions.height();
            if (hasAux) {
                if (p < pageNormalFilenames.size() && !pageNormalFilenames[p].isEmpty()) pObj["normal"] = pageNormalFilenames[p];
                if (p < pageSpecFilenames.size() && !pageSpecFilenames[p].isEmpty()) pObj["specular"] = pageSpecFilenames[p];
                if (p < pageEmissiveFilenames.size() && !pageEmissiveFilenames[p].isEmpty()) pObj["emissive"] = pageEmissiveFilenames[p];
            }
            pagesArr.append(pObj);
        }
        rootObj["pages"] = pagesArr;
    } else {
        rootObj["texture"] = pngFileName;
        if (hasAux) {
            QJsonObject texturesObj;
            texturesObj["diffuse"] = pngFileName;
            if (!normalFileName.isEmpty()) texturesObj["normal"] = normalFileName;
            if (!specFileName.isEmpty()) texturesObj["specular"] = specFileName;
            if (!emissiveFileName.isEmpty()) texturesObj["emissive"] = emissiveFileName;
            rootObj["textures"] = texturesObj;
        }
        QJsonObject texSize;
        texSize["w"] = packResult.dimensions.width();
        texSize["h"] = packResult.dimensions.height();
        rootObj["textureSize"] = texSize;
    }

    const int totalSprites = isMultiPage ? mpResult.frameRects.size() : packResult.frameRects.size();
    QJsonArray spritesArr;

    for (int i = 0; i < totalSprites; ++i) {
        QRect r = isMultiPage ? mpResult.rectForFrame(i) : packResult.frameRects[i];
        int pageIdx = isMultiPage ? mpResult.pageForFrame(i) : 0;
        double aW = isMultiPage ? mpResult.pages[pageIdx].dimensions.width() : packResult.dimensions.width();
        double aH = isMultiPage ? mpResult.pages[pageIdx].dimensions.height() : packResult.dimensions.height();

        QJsonObject sObj;
        QString sName = QStringLiteral("%1_%2").arg(baseName).arg(i, 4, 10, QLatin1Char('0'));
        sObj["name"] = sName;
        if (isMultiPage) {
            sObj["page"] = pageIdx;
            sObj["texture"] = pageFilenames[pageIdx];
        }

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
                QPolygonF boundaryPoly = box.polygon;
                if (boundaryPoly.isClosed() && boundaryPoly.size() >= 4) {
                    boundaryPoly.removeLast();
                }
                QJsonArray polyArr;
                for (const QPointF &pt : boundaryPoly) {
                    QJsonArray xy;
                    xy.append(pt.x());
                    xy.append(pt.y());
                    polyArr.append(xy);
                }
                sObj["polygon"] = polyArr;

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
            sObj["uv"] = uvArr;
            sObj["uvs"] = uvArr;
            sObj["triangles"] = tArr;

            if (!box.vertexWeights.isEmpty()) {
                QJsonArray wArr;
                QJsonArray cArr;
                for (float wt : box.vertexWeights) {
                    wArr.append(wt);
                    QJsonArray col;
                    col.append(wt);
                    col.append(wt);
                    col.append(wt);
                    col.append(1.0);
                    cArr.append(col);
                }
                sObj["weights"] = wArr;
                sObj["colors"] = cArr;
            }

            if (!box.sockets.isEmpty()) {
                QJsonObject socksObj;
                for (auto sit = box.sockets.constBegin(); sit != box.sockets.constEnd(); ++sit) {
                    QJsonObject ptObj;
                    ptObj["x"] = sit.value().x();
                    ptObj["y"] = sit.value().y();
                    socksObj[sit.key()] = ptObj;
                }
                sObj["sockets"] = socksObj;
            }
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
