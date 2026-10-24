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

#include "godotextractor.h"
#include "godot_pipeline.h"
#include "geometry/triangulator.h"
#include "packer/atlaspacker.h"
#include "packer/multiatlaspacker.h"
#include "packer/multipagepacker.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QSettings>
#include <QLabel>
#include <algorithm>

GodotExtractor::GodotExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool GodotExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() != QStringLiteral("tres")) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QString head = QString::fromUtf8(file.read(1024));
    file.close();
    return head.contains(QStringLiteral("SpriteFrames"));
}

bool GodotExtractor::read(const QString &filePath, SpriteDocument &doc, ExtractorError *error)
{
    setStatusMessage(tr("Reading Godot SpriteFrames resource..."));
    setProgress(10);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = tr("Cannot open Godot resource file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (!content.contains(QStringLiteral("SpriteFrames"))) {
        if (error) {
            error->code = ExtractorError::InvalidHeader;
            error->message = tr("File is not a valid Godot SpriteFrames resource: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(25);

    // 1. Locate referenced atlas texture image
    QRegularExpression extRegex(QStringLiteral(R"re(\[ext_resource\s+[^\]]*path="([^"]+)"[^\]]*\])re"));
    QRegularExpressionMatch extMatch = extRegex.match(content);
    QString rawImagePath;
    if (extMatch.hasMatch()) {
        rawImagePath = extMatch.captured(1);
    }
    if (rawImagePath.startsWith(QStringLiteral("res://"))) {
        rawImagePath = rawImagePath.mid(6);
    }

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString imagePath;

    if (!rawImagePath.isEmpty()) {
        if (dir.exists(rawImagePath)) {
            imagePath = dir.filePath(rawImagePath);
        } else {
            QString fname = QFileInfo(rawImagePath).fileName();
            if (dir.exists(fname)) {
                imagePath = dir.filePath(fname);
            }
        }
    }

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        QString candidateKtx2 = dir.filePath(fileInfo.completeBaseName() + QStringLiteral(".ktx2"));
        if (QFile::exists(candidateKtx2)) {
            imagePath = candidateKtx2;
        } else {
            QString candidatePng = dir.filePath(fileInfo.completeBaseName() + QStringLiteral(".png"));
            if (QFile::exists(candidatePng)) {
                imagePath = candidatePng;
            }
        }
    }

    if (imagePath.isEmpty() || !QFile::exists(imagePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Referenced texture atlas image not found for: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(40);

    QString loadErr;
    QImage atlasImg = VramTextureCompressor::loadAtlasImage(imagePath, &loadErr);
    if (atlasImg.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to load texture atlas image: %1").arg(imagePath);
            if (!loadErr.isEmpty()) {
                error->message += QStringLiteral(" (%1)").arg(loadErr);
            }
            error->filePath = imagePath;
        }
        return false;
    }

    setProgress(60);

    // 2. Parse sub_resources (AtlasTexture definitions)
    QRegularExpression subResBlockRegex(QStringLiteral(
        R"re(\[sub_resource\s+type="AtlasTexture"\s+id=(?:"([^"]+)"|([^\s\]]+))\]([^\[]*))re"
    ));
    QRegularExpression regionRegex(QStringLiteral(
        R"re(region\s*=\s*Rect2\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))re"
    ));
    QRegularExpression marginRegex(QStringLiteral(
        R"re(margin\s*=\s*Rect2\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))re"
    ));

    QMap<QString, int> subResToFrameIdx;
    QList<SpriteBox> boxes;
    QList<QImage> frames;

    QRegularExpressionMatchIterator iter = subResBlockRegex.globalMatch(content);
    int frameIndex = 0;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString subResId = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        QString body = match.captured(3);

        QRegularExpressionMatch regMatch = regionRegex.match(body);
        if (!regMatch.hasMatch()) continue;

        int rx = qRound(regMatch.captured(1).toDouble());
        int ry = qRound(regMatch.captured(2).toDouble());
        int rw = qRound(regMatch.captured(3).toDouble());
        int rh = qRound(regMatch.captured(4).toDouble());

        QRect boxRect(rx, ry, rw, rh);
        boxRect = boxRect.intersected(atlasImg.rect());
        if (boxRect.isEmpty()) continue;

        SpriteBox box;
        box.rect = boxRect;
        box.index = frameIndex;
        box.selected = false;

        QRegularExpressionMatch marginMatch = marginRegex.match(body);
        if (marginMatch.hasMatch()) {
            double ox = marginMatch.captured(1).toDouble();
            double oy = marginMatch.captured(2).toDouble();
            double cw = marginMatch.captured(3).toDouble();
            double ch = marginMatch.captured(4).toDouble();
            int px = qRound((cw / 2.0) - ox);
            int py = qRound(ch - oy);
            box.pivot = QPoint(px, py);
            box.hasCustomPivot = true;
        } else {
            box.pivot = QPoint(rw / 2, rh);
            box.hasCustomPivot = false;
        }

        boxes.append(box);

        QImage frameImg = atlasImg.copy(boxRect);
        frames.append(frameImg);

        subResToFrameIdx.insert(subResId, frameIndex);
        frameIndex++;
    }

    setProgress(75);

    // 3. Parse animations block
    QMap<QString, SpriteAnimation> parsedAnimations;

    // Matches both Godot .tres: "animations = [" and JSON: "\"animations\": ["
    QRegularExpression animsStartRegex(QStringLiteral(R"re((?:animations\s*=|"animations"\s*:)\s*\[)re"));
    QRegularExpressionMatch animsStartMatch = animsStartRegex.match(content);

    if (animsStartMatch.hasMatch()) {
        int arrayStart = animsStartMatch.capturedEnd();
        int bracketDepth = 1;
        int arrayEnd = -1;

        for (int i = arrayStart; i < content.length(); ++i) {
            QChar c = content.at(i);
            if (c == QLatin1Char('[')) {
                bracketDepth++;
            } else if (c == QLatin1Char(']')) {
                bracketDepth--;
                if (bracketDepth == 0) {
                    arrayEnd = i;
                    break;
                }
            }
        }

        QString animsArrayStr = content.mid(arrayStart, (arrayEnd != -1 ? arrayEnd : content.length()) - arrayStart);

        // Find each { ... } animation block inside the array
        int braceDepth = 0;
        int blockStart = -1;
        QList<QString> animBlocks;

        for (int i = 0; i < animsArrayStr.length(); ++i) {
            QChar c = animsArrayStr.at(i);
            if (c == QLatin1Char('{')) {
                if (braceDepth == 0) {
                    blockStart = i;
                }
                braceDepth++;
            } else if (c == QLatin1Char('}')) {
                braceDepth--;
                if (braceDepth == 0 && blockStart != -1) {
                    animBlocks.append(animsArrayStr.mid(blockStart, i - blockStart + 1));
                    blockStart = -1;
                }
            }
        }

        QRegularExpression nameRegex(QStringLiteral(R"re((?:"name"|name)\s*[:=]\s*(?:&?"([^"]+)"|([a-zA-Z0-9_ -]+)))re"));
        QRegularExpression speedRegex(QStringLiteral(R"re((?:"speed"|speed)\s*[:=]\s*([0-9.]+))re"));
        QRegularExpression loopRegex(QStringLiteral(R"re((?:"loop"|loop)\s*[:=]\s*(true|false))re"));
        QRegularExpression subResRefRegex(QStringLiteral(R"re((?:SubResource|ExtResource)\(\s*(?:"([^"]+)"|([a-zA-Z0-9_]+))\s*\))re"));

        for (int b = 0; b < animBlocks.size(); ++b) {
            const QString &block = animBlocks[b];
            SpriteAnimation anim;
            anim.name = QStringLiteral("anim_%1").arg(b);
            anim.fps = 12;
            anim.loop = true;

            QRegularExpressionMatch nm = nameRegex.match(block);
            if (nm.hasMatch()) {
                anim.name = nm.captured(1).isEmpty() ? nm.captured(2).trimmed() : nm.captured(1);
            }

            QRegularExpressionMatch sm = speedRegex.match(block);
            if (sm.hasMatch()) {
                anim.fps = qMax(1, qRound(sm.captured(1).toDouble()));
            }

            QRegularExpressionMatch lm = loopRegex.match(block);
            if (lm.hasMatch()) {
                anim.loop = (lm.captured(1) == QStringLiteral("true"));
            }

            QRegularExpressionMatchIterator fit = subResRefRegex.globalMatch(block);
            while (fit.hasNext()) {
                QRegularExpressionMatch fm = fit.next();
                QString refId = fm.captured(1).isEmpty() ? fm.captured(2) : fm.captured(1);
                if (subResToFrameIdx.contains(refId)) {
                    anim.frameIndices.append(subResToFrameIdx.value(refId));
                }
            }

            if (!anim.frameIndices.isEmpty()) {
                parsedAnimations.insert(anim.name, anim);
            }
        }
    }

    if (parsedAnimations.isEmpty() && !frames.isEmpty()) {
        SpriteAnimation defAnim;
        defAnim.name = QStringLiteral("default");
        defAnim.fps = 10;
        defAnim.loop = true;
        for (int i = 0; i < frames.size(); ++i) {
            defAnim.frameIndices.append(i);
        }
        parsedAnimations.insert(defAnim.name, defAnim);
    }

    // 3.5 Look for companion _mesh.tres file to restore polygon meshes if available
    QString meshTresPath = dir.filePath(fileInfo.completeBaseName() + QStringLiteral("_mesh.tres"));
    if (QFile::exists(meshTresPath)) {
        QFile meshFile(meshTresPath);
        if (meshFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString meshContent = QString::fromUtf8(meshFile.readAll());
            meshFile.close();

            QRegularExpression polyRegex(QStringLiteral(R"re(metadata/frame_(\d+)/polygon\s*=\s*PackedVector2Array\(([^)]*)\))re"));
            QRegularExpression vertRegex(QStringLiteral(R"re(metadata/frame_(\d+)/vertices\s*=\s*PackedVector2Array\(([^)]*)\))re"));
            QRegularExpression intRegex(QStringLiteral(R"re(metadata/frame_(\d+)/internal_vertex_count\s*=\s*(\d+))re"));
            QRegularExpression triRegex(QStringLiteral(R"re(metadata/frame_(\d+)/triangles\s*=\s*PackedInt32Array\(([^)]*)\))re"));
            QRegularExpression weightRegex(QStringLiteral(R"re(metadata/frame_(\d+)/weights\s*=\s*PackedFloat32Array\(([^)]*)\))re"));
            QRegularExpression sockRegex(QStringLiteral(R"re(metadata/frame_(\d+)/sockets\s*=\s*\{([^}]*)\})re"));
            QRegularExpression sockEntryRegex(QStringLiteral(R"re("([^"]+)"\s*:\s*Vector2\(\s*(-?[0-9.]+)\s*,\s*(-?[0-9.]+)\s*\))re"));

            struct FrameMeshEntry {
                QPolygonF poly;
                QList<QPointF> verts;
                QList<int> tris;
                QList<float> weights;
                QMap<QString, QPointF> sockets;
                int internalVertexCount = -1;
                bool hasPoly = false;
                bool hasVerts = false;
                bool hasWeights = false;
                bool hasSockets = false;
            };
            QMap<int, FrameMeshEntry> frameMeshes;

            QRegularExpressionMatchIterator pIter = polyRegex.globalMatch(meshContent);
            while (pIter.hasNext()) {
                QRegularExpressionMatch match = pIter.next();
                int idx = match.captured(1).toInt();
                QString coordsStr = match.captured(2).trimmed();
                if (idx >= 0 && idx < boxes.size() && !coordsStr.isEmpty()) {
                    QStringList parts = coordsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    QPolygonF poly;
                    for (int p = 0; p + 1 < parts.size(); p += 2) {
                        double x = parts[p].trimmed().toDouble();
                        double y = parts[p + 1].trimmed().toDouble();
                        poly.append(QPointF(x, y));
                    }
                    if (!poly.isEmpty()) {
                        frameMeshes[idx].poly = poly;
                        frameMeshes[idx].hasPoly = true;
                    }
                }
            }

            QRegularExpressionMatchIterator vIter = vertRegex.globalMatch(meshContent);
            while (vIter.hasNext()) {
                QRegularExpressionMatch match = vIter.next();
                int idx = match.captured(1).toInt();
                QString coordsStr = match.captured(2).trimmed();
                if (idx >= 0 && idx < boxes.size() && !coordsStr.isEmpty()) {
                    QStringList parts = coordsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    QList<QPointF> verts;
                    for (int p = 0; p + 1 < parts.size(); p += 2) {
                        double x = parts[p].trimmed().toDouble();
                        double y = parts[p + 1].trimmed().toDouble();
                        verts.append(QPointF(x, y));
                    }
                    if (!verts.isEmpty()) {
                        frameMeshes[idx].verts = verts;
                        frameMeshes[idx].hasVerts = true;
                    }
                }
            }

            QRegularExpressionMatchIterator intIter = intRegex.globalMatch(meshContent);
            while (intIter.hasNext()) {
                QRegularExpressionMatch match = intIter.next();
                int idx = match.captured(1).toInt();
                if (idx >= 0 && idx < boxes.size()) {
                    frameMeshes[idx].internalVertexCount = match.captured(2).toInt();
                }
            }

            QRegularExpressionMatchIterator tIter = triRegex.globalMatch(meshContent);
            while (tIter.hasNext()) {
                QRegularExpressionMatch match = tIter.next();
                int idx = match.captured(1).toInt();
                QString triStr = match.captured(2).trimmed();
                if (idx >= 0 && idx < boxes.size() && !triStr.isEmpty()) {
                    QStringList parts = triStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    QList<int> tris;
                    tris.reserve(parts.size());
                    for (const QString &s : parts) {
                        tris.append(s.trimmed().toInt());
                    }
                    frameMeshes[idx].tris = tris;
                }
            }

            QRegularExpressionMatchIterator wIter = weightRegex.globalMatch(meshContent);
            while (wIter.hasNext()) {
                QRegularExpressionMatch match = wIter.next();
                int idx = match.captured(1).toInt();
                QString weightsStr = match.captured(2).trimmed();
                if (idx >= 0 && idx < boxes.size() && !weightsStr.isEmpty()) {
                    QStringList parts = weightsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    QList<float> weights;
                    weights.reserve(parts.size());
                    for (const QString &s : parts) {
                        weights.append(s.trimmed().toFloat());
                    }
                    frameMeshes[idx].weights = weights;
                    frameMeshes[idx].hasWeights = true;
                }
            }

            QRegularExpressionMatchIterator sIter = sockRegex.globalMatch(meshContent);
            while (sIter.hasNext()) {
                QRegularExpressionMatch match = sIter.next();
                int idx = match.captured(1).toInt();
                QString body = match.captured(2);
                if (idx >= 0 && idx < boxes.size()) {
                    QRegularExpressionMatchIterator entryIter = sockEntryRegex.globalMatch(body);
                    QMap<QString, QPointF> socks;
                    while (entryIter.hasNext()) {
                        QRegularExpressionMatch eMatch = entryIter.next();
                        QString sName = eMatch.captured(1);
                        double sx = eMatch.captured(2).toDouble();
                        double sy = eMatch.captured(3).toDouble();
                        socks.insert(sName, QPointF(sx, sy));
                    }
                    if (!socks.isEmpty()) {
                        frameMeshes[idx].sockets = socks;
                        frameMeshes[idx].hasSockets = true;
                    }
                }
            }

            for (auto it = frameMeshes.constBegin(); it != frameMeshes.constEnd(); ++it) {
                int idx = it.key();
                const FrameMeshEntry &fme = it.value();
                if (fme.hasVerts && fme.hasPoly) {
                    boxes[idx].hasPolygonMesh = true;
                    boxes[idx].polygon = fme.poly;
                    boxes[idx].vertices = fme.verts;
                    boxes[idx].triangles = fme.tris;
                } else if (fme.hasPoly) {
                    boxes[idx].hasPolygonMesh = true;
                    QList<QPointF> allVerts = fme.poly.toList();
                    boxes[idx].vertices = allVerts;
                    boxes[idx].triangles = fme.tris;

                    if (fme.internalVertexCount > 0 && fme.internalVertexCount < fme.poly.size()) {
                        boxes[idx].polygon = fme.poly.mid(0, fme.poly.size() - fme.internalVertexCount);
                    } else if (!fme.tris.isEmpty()) {
                        // Recover perimeter boundary contour from legacy mesh where internal points were merged into polygon
                        boxes[idx].polygon = BentoPackGeometry::Triangulator::extractBoundaryContour(allVerts, fme.tris);
                    } else {
                        boxes[idx].polygon = fme.poly;
                    }
                }
                if (fme.hasWeights) {
                    boxes[idx].vertexWeights = fme.weights;
                }
                if (fme.hasSockets) {
                    boxes[idx].sockets = fme.sockets;
                }
            }
        }
    }

    // 4. Update SpriteDocument directly
    doc.clear();
    doc.setFilePath(filePath);
    doc.setAtlas(atlasImg);
    doc.setFrames(frames, boxes);
    for (auto ait = parsedAnimations.begin(); ait != parsedAnimations.end(); ++ait) {
        doc.setAnimation(ait.key(), ait.value().frameIndices, ait.value().fps, ait.value().loop);
    }

    setProgress(100);
    setStatusMessage(tr("Extracted %1 frames and %2 animations from Godot resource.")
                         .arg(frames.size())
                         .arg(parsedAnimations.size()));
    emit extractionFinished(frames.size());
    return true;
}

bool GodotExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    setStatusMessage(tr("Packing atlas for Godot..."));
    setProgress(20);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
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
    QString imageFilename = baseName + imageExt;
    if (options.extraParams.contains(QStringLiteral("sheet_path"))) {
        const QString customSheet = options.extraParams.value(QStringLiteral("sheet_path")).toString();
        if (!customSheet.isEmpty()) {
            imageFilename = QFileInfo(customSheet).fileName();
        }
    }
    QString imagePath = dir.filePath(imageFilename);
    QString tresPath = dir.filePath(baseName + ".tres");

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
                error->message = tr("Failed to pack atlas frames for Godot export.");
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

    setProgress(60);

    QList<QString> pageImageFilenames;
    QList<QString> pageNormalFilenames;
    QList<QString> pageSpecFilenames;
    QList<QString> pageEmissiveFilenames;

    if (isMultiPage) {
        for (int p = 0; p < mpResult.totalPages(); ++p) {
            const AtlasPage &page = mpResult.pages[p];
            QString pageImgFilename = QStringLiteral("%1_%2%3").arg(baseName).arg(p).arg(imageExt);
            QString pageImgPath = dir.filePath(pageImgFilename);
            pageImageFilenames.append(pageImgFilename);

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
                error->message = tr("Failed to write Godot atlas page image: %1").arg(pageImgPath);
                error->filePath = pageImgPath;
                return false;
            }

            // Auxiliary maps for page p
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
            saveOk = VramTextureCompressor::compressToFile(packResult.atlas, imagePath, vOpts, nullptr, &vErr);
            if (!saveOk && error) {
                error->code = ExtractorError::WriteFailed;
                error->message = tr("Failed to write Godot VRAM atlas texture: %1 (%2)").arg(imagePath, vErr);
                error->filePath = imagePath;
                return false;
            }
        } else {
            saveOk = packResult.atlas.save(imagePath, "PNG");
            if (!saveOk && error) {
                error->code = ExtractorError::WriteFailed;
                error->message = tr("Failed to write Godot atlas image: %1").arg(imagePath);
                error->filePath = imagePath;
                return false;
            }
        }
    }

    // Save synchronized auxiliary material maps if present (single-page mode)
    QString normalFilename, specFilename, emissiveFilename;
    if (!isMultiPage && hasAux) {
        if (multiResult.hasMap(MaterialMapType::Normal)) {
            normalFilename = baseName + QStringLiteral("_n") + imageExt;
            multiResult.maps[MaterialMapType::Normal].save(dir.filePath(normalFilename), "PNG");
        }
        if (multiResult.hasMap(MaterialMapType::Specular)) {
            specFilename = baseName + QStringLiteral("_s") + imageExt;
            multiResult.maps[MaterialMapType::Specular].save(dir.filePath(specFilename), "PNG");
        }
        if (multiResult.hasMap(MaterialMapType::Emissive)) {
            emissiveFilename = baseName + QStringLiteral("_e") + imageExt;
            multiResult.maps[MaterialMapType::Emissive].save(dir.filePath(emissiveFilename), "PNG");
        }
    }

    setProgress(80);

    QFile outFile(tresPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to Godot resource file: %1").arg(tresPath);
            error->filePath = tresPath;
        }
        return false;
    }

    QTextStream out(&outFile);

    // Resolve Godot 4 UID
    QString uid = options.extraParams.value(QStringLiteral("godot_uid")).toString();
    bool preserveUid = options.extraParams.value(QStringLiteral("preserve_uid"), true).toBool();
    if (uid.isEmpty() && preserveUid && QFile::exists(tresPath)) {
        uid = GodotPipeline::extractExistingUid(tresPath);
    }
    if (uid.isEmpty()) {
        uid = GodotPipeline::generateDeterministicUid(fileInfo.fileName());
    }

    // Write Godot 4.x SpriteFrames header with optional CanvasTexture 2D Lighting
    bool useCanvasTexture = hasAux && (!normalFilename.isEmpty() || !specFilename.isEmpty() || !pageNormalFilenames.isEmpty());
    const int totalFramesToOutput = isMultiPage ? mpResult.frameRects.size() : packResult.frameRects.size();

    int extResourceCount = isMultiPage ? pageImageFilenames.size() : 1;
    if (useCanvasTexture) {
        if (isMultiPage) {
            for (const QString &n : pageNormalFilenames) if (!n.isEmpty()) extResourceCount++;
            for (const QString &s : pageSpecFilenames) if (!s.isEmpty()) extResourceCount++;
        } else {
            if (!normalFilename.isEmpty()) extResourceCount++;
            if (!specFilename.isEmpty()) extResourceCount++;
        }
    }

    int loadSteps = totalFramesToOutput + extResourceCount + 1;
    if (useCanvasTexture) loadSteps += (isMultiPage ? mpResult.totalPages() : 1);

    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << loadSteps << " format=3";
    if (!uid.isEmpty()) {
        out << " uid=\"" << uid << "\"";
    }
    out << "]\n\n";

    if (isMultiPage) {
        // Multi-page declarations
        for (int p = 0; p < mpResult.totalPages(); ++p) {
            out << "[ext_resource type=\"Texture2D\" path=\"res://" << pageImageFilenames[p] << "\" id=\"page_" << p << "_diffuse\"]\n";
            if (useCanvasTexture && p < pageNormalFilenames.size() && !pageNormalFilenames[p].isEmpty()) {
                out << "[ext_resource type=\"Texture2D\" path=\"res://" << pageNormalFilenames[p] << "\" id=\"page_" << p << "_normal\"]\n";
            }
            if (useCanvasTexture && p < pageSpecFilenames.size() && !pageSpecFilenames[p].isEmpty()) {
                out << "[ext_resource type=\"Texture2D\" path=\"res://" << pageSpecFilenames[p] << "\" id=\"page_" << p << "_specular\"]\n";
            }
        }
        out << "\n";

        if (useCanvasTexture) {
            for (int p = 0; p < mpResult.totalPages(); ++p) {
                out << "[sub_resource type=\"CanvasTexture\" id=\"CanvasTexture_page_" << p << "\"]\n";
                out << "diffuse_texture = ExtResource(\"page_" << p << "_diffuse\")\n";
                if (p < pageNormalFilenames.size() && !pageNormalFilenames[p].isEmpty()) {
                    out << "normal_texture = ExtResource(\"page_" << p << "_normal\")\n";
                }
                if (p < pageSpecFilenames.size() && !pageSpecFilenames[p].isEmpty()) {
                    out << "specular_texture = ExtResource(\"page_" << p << "_specular\")\n";
                }
                out << "\n";
            }
        }

        for (int i = 0; i < totalFramesToOutput; ++i) {
            int p = mpResult.pageForFrame(i);
            QRect r = mpResult.rectForFrame(i);
            QString subResId = QString("AtlasTexture_%1").arg(i);
            out << "[sub_resource type=\"AtlasTexture\" id=\"" << subResId << "\"]\n";
            if (useCanvasTexture) {
                out << "atlas = SubResource(\"CanvasTexture_page_" << p << "\")\n";
            } else {
                out << "atlas = ExtResource(\"page_" << p << "_diffuse\")\n";
            }
            out << "region = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n";

            QString animName;
            for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
                if (it.value().frameIndices.contains(i)) {
                    animName = it.key();
                    break;
                }
            }
            QRect env = doc.computeAnimationEnvelope(animName);
            QPoint piv = doc.boxPivot(i);
            int ox = env.x() - piv.x();
            int oy = env.y() - piv.y();
            if (env.width() > r.width() || env.height() > r.height() || ox != 0 || oy != 0) {
                out << "margin = Rect2(" << ox << ", " << oy << ", " << env.width() << ", " << env.height() << ")\n";
            }
            out << "\n";
        }
    } else {
        // Single-page declaration (backward-compatible)
        if (useCanvasTexture) {
            out << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_diffuse\"]\n";
            int nextId = 2;
            QString normIdStr, specIdStr;
            if (!normalFilename.isEmpty()) {
                normIdStr = QStringLiteral("%1_normal").arg(nextId++);
                out << "[ext_resource type=\"Texture2D\" path=\"res://" << normalFilename << "\" id=\"" << normIdStr << "\"]\n";
            }
            if (!specFilename.isEmpty()) {
                specIdStr = QStringLiteral("%1_specular").arg(nextId++);
                out << "[ext_resource type=\"Texture2D\" path=\"res://" << specFilename << "\" id=\"" << specIdStr << "\"]\n";
            }
            out << "\n";
            out << "[sub_resource type=\"CanvasTexture\" id=\"CanvasTexture_main\"]\n";
            out << "diffuse_texture = ExtResource(\"1_diffuse\")\n";
            if (!normIdStr.isEmpty()) {
                out << "normal_texture = ExtResource(\"" << normIdStr << "\")\n";
            }
            if (!specIdStr.isEmpty()) {
                out << "specular_texture = ExtResource(\"" << specIdStr << "\")\n";
            }
            out << "\n";
        } else {
            out << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";
        }

        for (int i = 0; i < packResult.frameRects.size(); ++i) {
            const QRect &r = packResult.frameRects[i];
            QString subResId = QString("AtlasTexture_%1").arg(i);
            out << "[sub_resource type=\"AtlasTexture\" id=\"" << subResId << "\"]\n";
            if (useCanvasTexture) {
                out << "atlas = SubResource(\"CanvasTexture_main\")\n";
            } else {
                out << "atlas = ExtResource(\"1_atlas\")\n";
            }
            out << "region = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n";

            // Find which animation contains frame i
            QString animName;
            for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
                if (it.value().frameIndices.contains(i)) {
                    animName = it.key();
                    break;
                }
            }
            QRect env = doc.computeAnimationEnvelope(animName);
            QPoint piv = doc.boxPivot(i);
            int ox = env.x() - piv.x();
            int oy = env.y() - piv.y();
            if (env.width() > r.width() || env.height() > r.height() || ox != 0 || oy != 0) {
                out << "margin = Rect2(" << ox << ", " << oy << ", " << env.width() << ", " << env.height() << ")\n";
            }
            out << "\n";
        }
    }

    out << "[resource]\n";
    out << "animations = [{\n";

    auto animations = doc.animations();
    if (animations.isEmpty()) {
        out << "\"frames\": [";
        for (int i = 0; i < packResult.frameRects.size(); ++i) {
            if (i > 0) out << ", ";
            out << "SubResource(\"AtlasTexture_" << i << "\")";
        }
        out << "],\n";
        out << "\"loop\": true,\n";
        out << "\"name\": &\"default\",\n";
        out << "\"speed\": 10.0\n";
        out << "}]\n";
    } else {
        bool firstAnim = true;
        for (auto it = animations.begin(); it != animations.end(); ++it) {
            if (!firstAnim) out << "}, {\n";
            firstAnim = false;

            out << "\"frames\": [";
            const QList<int> &fIndices = it.value().frameIndices;
            for (int j = 0; j < fIndices.size(); ++j) {
                if (j > 0) out << ", ";
                out << "SubResource(\"AtlasTexture_" << fIndices[j] << "\")";
            }
            out << "],\n";
            out << "\"loop\": " << (it.value().loop ? "true" : "false") << ",\n";
            out << "\"name\": &\"" << it.key() << "\",\n";
            out << "\"speed\": " << static_cast<double>(it.value().fps) << ".0\n";
        }
        out << "}]\n";
    }

    outFile.close();

    // If any frame has a tight polygon mesh, weights, or sockets, write companion _mesh.tres resource
    bool hasMesh = false;
    for (int i = 0; i < doc.frameCount(); ++i) {
        const SpriteBox &bx = doc.box(i);
        if ((bx.hasPolygonMesh && !bx.polygon.isEmpty() && !bx.triangles.isEmpty()) ||
            !bx.vertexWeights.isEmpty() || !bx.sockets.isEmpty()) {
            hasMesh = true;
            break;
        }
    }

    if (hasMesh) {
        QString meshTresPath = dir.filePath(baseName + "_mesh.tres");
        QFile meshFile(meshTresPath);
        if (meshFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream mOut(&meshFile);
            mOut << "[gd_resource type=\"Resource\" format=3]\n\n";
            if (isMultiPage) {
                for (int p = 0; p < mpResult.totalPages(); ++p) {
                    mOut << "[ext_resource type=\"Texture2D\" path=\"res://" << pageImageFilenames[p] << "\" id=\"page_" << p << "_atlas\"]\n";
                }
                mOut << "\n[resource]\n";
                mOut << "metadata/pages_count = " << mpResult.totalPages() << "\n";
            } else {
                mOut << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";
                mOut << "[resource]\n";
                mOut << "metadata/texture = ExtResource(\"1_atlas\")\n";
                mOut << "metadata/atlas_width = " << packResult.dimensions.width() << "\n";
                mOut << "metadata/atlas_height = " << packResult.dimensions.height() << "\n";
            }

            for (int i = 0; i < totalFramesToOutput; ++i) {
                QRect r = isMultiPage ? mpResult.rectForFrame(i) : packResult.frameRects[i];
                int pageIdx = isMultiPage ? mpResult.pageForFrame(i) : 0;
                double aW = isMultiPage ? mpResult.pages[pageIdx].dimensions.width() : packResult.dimensions.width();
                double aH = isMultiPage ? mpResult.pages[pageIdx].dimensions.height() : packResult.dimensions.height();

                if (i < doc.frameCount()) {
                    const SpriteBox &b = doc.box(i);
                    bool hasGeo = b.hasPolygonMesh && !b.polygon.isEmpty() && !b.triangles.isEmpty();
                    if (hasGeo || !b.vertexWeights.isEmpty() || !b.sockets.isEmpty()) {
                        mOut << "metadata/frame_" << i << "/rect = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n";
                        if (isMultiPage) {
                            mOut << "metadata/frame_" << i << "/page = " << pageIdx << "\n";
                            mOut << "metadata/frame_" << i << "/texture = ExtResource(\"page_" << pageIdx << "_atlas\")\n";
                        }

                        if (hasGeo) {
                            // 1. Clean boundary perimeter contour
                            QPolygonF boundaryPoly = b.polygon;
                            if (boundaryPoly.isClosed() && boundaryPoly.size() >= 4) {
                                boundaryPoly.removeLast();
                            }

                            // 2. Full mesh vertices (boundary first, followed by interior Steiner/contrast points)
                            const QList<QPointF> meshVertices = !b.vertices.isEmpty() ? b.vertices : boundaryPoly.toList();
                            const int internalVertexCount = std::max(0, static_cast<int>(meshVertices.size()) - static_cast<int>(boundaryPoly.size()));

                            // Boundary polygon coordinates
                            mOut << "metadata/frame_" << i << "/polygon = PackedVector2Array(";
                            for (int p = 0; p < boundaryPoly.size(); ++p) {
                                if (p > 0) mOut << ", ";
                                mOut << boundaryPoly[p].x() << ", " << boundaryPoly[p].y();
                            }
                            mOut << ")\n";

                            // Full mesh vertices (when mesh contains interior vertices or differs from boundary)
                            if (internalVertexCount > 0 || meshVertices.size() != boundaryPoly.size()) {
                                mOut << "metadata/frame_" << i << "/vertices = PackedVector2Array(";
                                for (int p = 0; p < meshVertices.size(); ++p) {
                                    if (p > 0) mOut << ", ";
                                    mOut << meshVertices[p].x() << ", " << meshVertices[p].y();
                                }
                                mOut << ")\n";
                                mOut << "metadata/frame_" << i << "/internal_vertex_count = " << internalVertexCount << "\n";
                            }

                            // UV coordinates (mapped to meshVertices)
                            mOut << "metadata/frame_" << i << "/uv = PackedVector2Array(";
                            for (int p = 0; p < meshVertices.size(); ++p) {
                                if (p > 0) mOut << ", ";
                                double u = (aW > 0.0) ? ((r.x() + meshVertices[p].x()) / aW) : 0.0;
                                double v = (aH > 0.0) ? ((r.y() + meshVertices[p].y()) / aH) : 0.0;
                                mOut << u << ", " << v;
                            }
                            mOut << ")\n";

                            // Triangles (indices into meshVertices)
                            mOut << "metadata/frame_" << i << "/triangles = PackedInt32Array(";
                            for (int t = 0; t < b.triangles.size(); ++t) {
                                if (t > 0) mOut << ", ";
                                mOut << b.triangles[t];
                            }
                            mOut << ")\n";

                            // Vertex weights & colors
                            if (!b.vertexWeights.isEmpty()) {
                                mOut << "metadata/frame_" << i << "/weights = PackedFloat32Array(";
                                for (int w = 0; w < b.vertexWeights.size(); ++w) {
                                    if (w > 0) mOut << ", ";
                                    mOut << b.vertexWeights[w];
                                }
                                mOut << ")\n";

                                mOut << "metadata/frame_" << i << "/colors = PackedColorArray(";
                                for (int w = 0; w < b.vertexWeights.size(); ++w) {
                                    if (w > 0) mOut << ", ";
                                    float wt = b.vertexWeights[w];
                                    mOut << "Color(" << wt << ", " << wt << ", " << wt << ", 1)";
                                }
                                mOut << ")\n";
                            }
                        }

                        // Sockets
                        if (!b.sockets.isEmpty()) {
                            mOut << "metadata/frame_" << i << "/sockets = {";
                            bool firstSock = true;
                            for (auto sit = b.sockets.constBegin(); sit != b.sockets.constEnd(); ++sit) {
                                if (!firstSock) mOut << ", ";
                                firstSock = false;
                                mOut << "\"" << sit.key() << "\": Vector2(" << sit.value().x() << ", " << sit.value().y() << ")";
                            }
                            mOut << "}\n";
                        }
                    }
                }
            }
            meshFile.close();
        }
    }

    // If requested, generate companion Godot AnimatedSprite2D scene (.tscn)
    QString scenePath = options.extraParams.value(QStringLiteral("godot_scene")).toString();
    if (!scenePath.isEmpty()) {
        QString sErr;
        QString defaultAnim = doc.animations().isEmpty() ? QStringLiteral("default") : doc.animations().firstKey();
        GodotPipeline::generateScene(scenePath, tresPath, defaultAnim, &sErr);
    }

    setProgress(100);
    setStatusMessage(tr("Exported Godot resource: %1 and image %2").arg(QFileInfo(tresPath).fileName(), imageFilename));
    return true;
}

QWidget* GodotExtractor::createSettingsWidget(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    QSettings settings;
    settings.beginGroup(QStringLiteral("Plugins/GodotExtractor"));
    bool preserveUid = settings.value(QStringLiteral("preserve_uid"), true).toBool();
    bool genScene = settings.value(QStringLiteral("generate_scene"), false).toBool();
    settings.endGroup();

    QCheckBox *chkUid = new QCheckBox(tr("Preserve existing Godot 4 UID (uid://...) on re-export"), w);
    chkUid->setChecked(preserveUid);
    QObject::connect(chkUid, &QCheckBox::toggled, [](bool checked) {
        QSettings s;
        s.beginGroup(QStringLiteral("Plugins/GodotExtractor"));
        s.setValue(QStringLiteral("preserve_uid"), checked);
        s.endGroup();
    });
    layout->addWidget(chkUid);

    QCheckBox *chkScene = new QCheckBox(tr("Auto-generate AnimatedSprite2D scene (.tscn) companion file"), w);
    chkScene->setChecked(genScene);
    QObject::connect(chkScene, &QCheckBox::toggled, [](bool checked) {
        QSettings s;
        s.beginGroup(QStringLiteral("Plugins/GodotExtractor"));
        s.setValue(QStringLiteral("generate_scene"), checked);
        s.endGroup();
    });
    layout->addWidget(chkScene);

    QLabel *info = new QLabel(tr("Godot Engine 4.x SpriteFrames exporter with AtlasTexture regions and collision mesh generation."), w);
    info->setWordWrap(true);
    info->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    layout->addWidget(info);

    return w;
}
