#include "jsonextractor.h"
#include "jsonExtractordialog.h"
#include "packer/atlaspacker.h"
#include "geometry/triangulator.h"
#include <QDebug>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>

JsonExtractor::JsonExtractor(QObject *parent)
    : Extractor(parent)
{
}

bool JsonExtractor::canDecode(const QString &filePath) const
{
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() != QStringLiteral("json")) {
        return false;
    }
    if (filePath.endsWith(QStringLiteral(".unity.json"), Qt::CaseInsensitive) ||
        filePath.endsWith(QStringLiteral(".paper2d.json"), Qt::CaseInsensitive)) {
        return false;
    }

    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray head = jsonFile.read(2048);
    jsonFile.close();

    // Check for standard atlas JSON tags
    return head.contains("frames") || head.contains("meta");
}

static bool getJsonRoot(const QString &filePath, QJsonObject &dest, ExtractorError *error = nullptr)
{
    QFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        if (error) {
            error->code = ExtractorError::FileNotFound;
            error->message = QObject::tr("Cannot open JSON file: %1").arg(jsonFile.errorString());
            error->filePath = filePath;
        }
        return false;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            error->code = ExtractorError::ParsingFailed;
            error->message = QObject::tr("JSON parse error: %1 at offset %2").arg(parseError.errorString()).arg(parseError.offset);
            error->filePath = filePath;
        }
        return false;
    }

    if (!doc.isObject()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = QObject::tr("JSON root must be an object.");
            error->filePath = filePath;
        }
        return false;
    }

    dest = doc.object();
    return true;
}

static QStringList findFilesGlob(const QString &path, const QString &filter)
{
    QStringList res;
    QDir dir(path);
    QStringList name_filters;
    name_filters << filter;
    QFileInfoList fil = dir.entryInfoList(name_filters, QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    for (const QFileInfo &fi : fil) {
        if (fi.isFile()) res.push_back(fi.fileName());
    }
    return res;
}

bool JsonExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    setStatusMessage(tr("Reading JSON sprite atlas %1...").arg(QFileInfo(filePath).fileName()));
    setProgress(5);

    QJsonObject root;
    if (!getJsonRoot(filePath, root, error)) {
        return false;
    }

    // Locate companion atlas image
    QString imageFileName;
    if (root.contains("meta") && root["meta"].isObject()) {
        QJsonObject meta = root["meta"].toObject();
        if (meta.contains("image") && meta["image"].isString()) {
            imageFileName = meta["image"].toString();
        }
    }

    QFileInfo jsonFileInfo(filePath);
    QString imageFilePath;

    if (!imageFileName.isEmpty()) {
        imageFilePath = jsonFileInfo.dir().filePath(imageFileName);
        if (!QFile::exists(imageFilePath)) {
            QString candKtx2 = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + QStringLiteral(".ktx2"));
            if (QFile::exists(candKtx2)) {
                imageFilePath = candKtx2;
            } else {
                imageFilePath = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + QStringLiteral(".png"));
            }
        }
    } else {
        QString candKtx2 = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + QStringLiteral(".ktx2"));
        if (QFile::exists(candKtx2)) {
            imageFilePath = candKtx2;
        } else {
            imageFilePath = jsonFileInfo.dir().filePath(jsonFileInfo.completeBaseName() + QStringLiteral(".png"));
        }
    }

    if (!QFile::exists(imageFilePath)) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Associated atlas image not found: %1").arg(imageFilePath);
            error->filePath = imageFilePath;
        }
        return false;
    }

    QString loadErr;
    QImage atlasImage = VramTextureCompressor::loadAtlasImage(imageFilePath, &loadErr);
    if (atlasImage.isNull()) {
        if (error) {
            error->code = ExtractorError::ImageLoadFailed;
            error->message = tr("Failed to decode atlas image: %1").arg(imageFilePath);
            if (!loadErr.isEmpty()) {
                error->message += QStringLiteral(" (%1)").arg(loadErr);
            }
            error->filePath = imageFilePath;
        }
        return false;
    }

    setProgress(30);

    QList<QImage> frames;
    QList<SpriteBox> boxes;
    QMap<QString, QList<int>> animationFrames;

    // Check for friend multi-file animations (e.g. project-walk.json)
    QFileInfo fileInfo(imageFilePath);
    QStringList friendAnimations = findFilesGlob(fileInfo.absolutePath(), fileInfo.baseName() + "-*.json");
    if (friendAnimations.isEmpty()) {
        friendAnimations.push_back(jsonFileInfo.fileName());
    }

    for (const QString &currentAnimation : friendAnimations) {
        QString subJsonPath = fileInfo.absolutePath() + QDir::separator() + currentAnimation;
        QJsonObject currentRoot;
        if (!getJsonRoot(subJsonPath, currentRoot, nullptr)) {
            continue;
        }

        if (currentRoot.contains("frames")) {
            if (currentRoot["frames"].isObject()) {
                extractFromTexturePackerFormat(currentRoot["frames"].toObject(), atlasImage, frames, boxes, animationFrames);
            } else if (currentRoot["frames"].isArray()) {
                extractFromArrayFormat(currentRoot["frames"].toArray(), atlasImage, frames, boxes, animationFrames);
            }
        }

        if (currentRoot.contains("meta") && currentRoot["meta"].isObject()) {
            QJsonObject meta = currentRoot["meta"].toObject();
            if (meta.contains("frameTags") && meta["frameTags"].isArray()) {
                QMap<QString, QList<int>> tagAnimations;
                extractAnimationsFromFrameTags(meta["frameTags"].toArray(), tagAnimations, frames.size());
                if (!tagAnimations.isEmpty()) {
                    animationFrames = tagAnimations;
                }
            }
        }
    }

    if (frames.isEmpty()) {
        if (error) {
            error->code = ExtractorError::CorruptedData;
            error->message = tr("No frames could be extracted from JSON: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    // Default animation fallback if none parsed
    QMap<QString, SpriteAnimation> parsedAnimations;
    if (animationFrames.isEmpty()) {
        SpriteAnimation defAnim;
        defAnim.name = jsonFileInfo.completeBaseName();
        defAnim.fps = 12;
        defAnim.loop = true;
        for (int i = 0; i < frames.size(); ++i) {
            defAnim.frameIndices.append(i);
        }
        parsedAnimations.insert(defAnim.name, defAnim);
    } else {
        for (auto it = animationFrames.begin(); it != animationFrames.end(); ++it) {
            SpriteAnimation anim;
            anim.name = it.key();
            anim.frameIndices = it.value();
            anim.fps = 12;
            anim.loop = true;
            parsedAnimations.insert(anim.name, anim);
        }
    }

    // Populate SpriteDocument directly
    outDoc.setFilePath(filePath);
    outDoc.setAtlas(atlasImage);
    outDoc.setFrames(frames, boxes);
    for (auto ait = parsedAnimations.begin(); ait != parsedAnimations.end(); ++ait) {
        outDoc.setAnimation(ait.key(), ait.value().frameIndices, ait.value().fps, ait.value().loop);
    }

    setProgress(100);
    setStatusMessage(tr("Imported %1 frames, %2 animations from JSON").arg(frames.size()).arg(parsedAnimations.size()));
    emit extractionFinished(frames.size());
    return true;
}

void JsonExtractor::extractFromTexturePackerFormat(const QJsonObject &framesObj,
                                                  const QImage &atlasImage,
                                                  QList<QImage> &frames,
                                                  QList<SpriteBox> &boxes,
                                                  QMap<QString, QList<int>> &animationFrames)
{
    int baseIndex = frames.size();

    for (auto it = framesObj.begin(); it != framesObj.end(); ++it) {
        QString frameName = it.key();
        QJsonValue frameValue = it.value();
        if (!frameValue.isObject()) continue;

        QJsonObject frameObj = frameValue.toObject();
        if (!frameObj.contains("frame") || !frameObj["frame"].isObject()) continue;

        QJsonObject frameRect = frameObj["frame"].toObject();
        int x = frameRect.value("x").toInt();
        int y = frameRect.value("y").toInt();
        int w = frameRect.value("w").toInt();
        int h = frameRect.value("h").toInt();

        if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x + w > atlasImage.width() || y + h > atlasImage.height()) {
            continue;
        }

        QImage frameImg = atlasImage.copy(x, y, w, h);
        int currentIndex = baseIndex + frames.size();

        SpriteBox box;
        box.rect = QRect(x, y, w, h);
        box.selected = false;
        box.index = currentIndex;

        if (frameObj.contains("pivot") && frameObj["pivot"].isObject()) {
            QJsonObject pObj = frameObj["pivot"].toObject();
            double normX = pObj.value("x").toDouble(0.5);
            double normY = pObj.value("y").toDouble(1.0);
            box.pivot = QPoint(qRound(normX * w), qRound(normY * h));
            box.hasCustomPivot = true;
        } else {
            box.pivot = QPoint(w / 2, h);
            box.hasCustomPivot = false;
        }

        if (frameObj.contains("vertices") && frameObj["vertices"].isArray()) {
            QPolygonF poly;
            QJsonArray vArr = frameObj["vertices"].toArray();
            for (const QJsonValue &vVal : vArr) {
                if (vVal.isArray()) {
                    QJsonArray xy = vVal.toArray();
                    if (xy.size() >= 2) {
                        poly.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
                    }
                }
            }
            if (poly.size() >= 3) {
                box.polygon = poly;
                box.vertices = poly.toList();

                QList<int> tris;
                if (frameObj.contains("triangles") && frameObj["triangles"].isArray()) {
                    QJsonArray tArr = frameObj["triangles"].toArray();
                    for (const QJsonValue &tVal : tArr) {
                        if (tVal.isArray()) {
                            QJsonArray tri = tVal.toArray();
                            for (const QJsonValue &idxVal : tri) {
                                tris.append(idxVal.toInt());
                            }
                        } else if (tVal.isDouble()) {
                            tris.append(tVal.toInt());
                        }
                    }
                }
                if (tris.isEmpty() || tris.size() % 3 != 0) {
                    tris = SpriteStudioGeometry::Triangulator::triangulate(poly);
                }
                box.triangles = tris;
                box.hasPolygonMesh = true;
            }
        }

        frames.append(frameImg);
        boxes.append(box);

        QString animName = extractAnimationName(frameName);
        if (!animName.isEmpty()) {
            animationFrames[animName].append(currentIndex);
        }
    }
}

void JsonExtractor::extractFromArrayFormat(const QJsonArray &framesArray,
                                          const QImage &atlasImage,
                                          QList<QImage> &frames,
                                          QList<SpriteBox> &boxes,
                                          QMap<QString, QList<int>> &animationFrames)
{
    int baseIndex = frames.size();

    for (int i = 0; i < framesArray.size(); ++i) {
        QJsonValue frameValue = framesArray[i];
        if (!frameValue.isObject()) continue;

        QJsonObject frameObj = frameValue.toObject();
        QJsonObject frameRect;
        if (frameObj.contains("frame") && frameObj["frame"].isObject()) {
            frameRect = frameObj["frame"].toObject();
        } else if (frameObj.contains("x") && frameObj.contains("y") &&
                   frameObj.contains("w") && frameObj.contains("h")) {
            frameRect = frameObj;
        } else {
            continue;
        }

        int x = frameRect.value("x").toInt();
        int y = frameRect.value("y").toInt();
        int w = frameRect.value("w").toInt();
        int h = frameRect.value("h").toInt();

        if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
            x + w > atlasImage.width() || y + h > atlasImage.height()) {
            continue;
        }

        QImage frameImg = atlasImage.copy(x, y, w, h);
        int currentIndex = baseIndex + frames.size();

        SpriteBox box;
        box.rect = QRect(x, y, w, h);
        box.selected = false;
        box.index = currentIndex;

        if (frameObj.contains("pivot") && frameObj["pivot"].isObject()) {
            QJsonObject pObj = frameObj["pivot"].toObject();
            double normX = pObj.value("x").toDouble(0.5);
            double normY = pObj.value("y").toDouble(1.0);
            box.pivot = QPoint(qRound(normX * w), qRound(normY * h));
            box.hasCustomPivot = true;
        } else {
            box.pivot = QPoint(w / 2, h);
            box.hasCustomPivot = false;
        }

        if (frameObj.contains("vertices") && frameObj["vertices"].isArray()) {
            QPolygonF poly;
            QJsonArray vArr = frameObj["vertices"].toArray();
            for (const QJsonValue &vVal : vArr) {
                if (vVal.isArray()) {
                    QJsonArray xy = vVal.toArray();
                    if (xy.size() >= 2) {
                        poly.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
                    }
                }
            }
            if (poly.size() >= 3) {
                box.polygon = poly;
                box.vertices = poly.toList();

                QList<int> tris;
                if (frameObj.contains("triangles") && frameObj["triangles"].isArray()) {
                    QJsonArray tArr = frameObj["triangles"].toArray();
                    for (const QJsonValue &tVal : tArr) {
                        if (tVal.isArray()) {
                            QJsonArray tri = tVal.toArray();
                            for (const QJsonValue &idxVal : tri) {
                                tris.append(idxVal.toInt());
                            }
                        } else if (tVal.isDouble()) {
                            tris.append(tVal.toInt());
                        }
                    }
                }
                if (tris.isEmpty() || tris.size() % 3 != 0) {
                    tris = SpriteStudioGeometry::Triangulator::triangulate(poly);
                }
                box.triangles = tris;
                box.hasPolygonMesh = true;
            }
        }

        frames.append(frameImg);
        boxes.append(box);

        if (frameObj.contains("filename") && frameObj["filename"].isString()) {
            QString filename = frameObj["filename"].toString();
            QString animName = extractAnimationName(filename);
            if (!animName.isEmpty()) {
                animationFrames[animName].append(currentIndex);
            }
        }
    }
}

void JsonExtractor::extractAnimationsFromFrameTags(const QJsonArray &frameTagsArray,
                                                  QMap<QString, QList<int>> &animationFrames,
                                                  int totalFrames)
{
    for (const QJsonValue &tagValue : frameTagsArray) {
        if (!tagValue.isObject()) continue;

        QJsonObject tagObj = tagValue.toObject();
        if (!tagObj.contains("name") || !tagObj["name"].isString() ||
            !tagObj.contains("from") || !tagObj.contains("to")) {
            continue;
        }

        QString animName = tagObj["name"].toString();
        int from = tagObj["from"].toInt();
        int to = tagObj["to"].toInt();

        QList<int> frameSeq;
        for (int i = from; i <= to; ++i) {
            if (i >= 0 && i < totalFrames) {
                frameSeq.append(i);
            }
        }

        if (!frameSeq.isEmpty()) {
            animationFrames[animName] = frameSeq;
        }
    }
}

QString JsonExtractor::extractAnimationName(const QString &frameName)
{
    QRegularExpression regex(QStringLiteral(R"re(^([a-zA-Z0-9_-]+)[_/]\d+$)re"));
    QRegularExpressionMatch match = regex.match(frameName);
    if (match.hasMatch()) {
        QString animName = match.captured(1);
        if (animName.toLower() != QStringLiteral("frame")) {
            return animName;
        }
    }
    return QString();
}

bool JsonExtractor::write(const QString &filePath, const SpriteDocument &doc, const ExportOptions &options, ExtractorError *error)
{
    if (doc.frameCount() == 0) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("No frames in document to export.");
            error->filePath = filePath;
        }
        return false;
    }

    QFileInfo fi(filePath);
    QDir dir = fi.dir();
    QString baseName = fi.completeBaseName();
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

    setStatusMessage(tr("Packing atlas for JSON export..."));
    setProgress(20);

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
            error->message = tr("Failed to pack frames for JSON export.");
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

    // Build TexturePacker compatible JSON
    QJsonObject rootObj;
    QJsonObject framesObj;

    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QJsonObject frameData;

        QJsonObject fRect;
        fRect["x"] = r.x();
        fRect["y"] = r.y();
        fRect["w"] = r.width();
        fRect["h"] = r.height();
        frameData["frame"] = fRect;
        frameData["rotated"] = false;
        frameData["trimmed"] = false;

        QJsonObject sRect;
        sRect["x"] = 0;
        sRect["y"] = 0;
        sRect["w"] = r.width();
        sRect["h"] = r.height();
        frameData["spriteSourceSize"] = sRect;

        QJsonObject srcSize;
        srcSize["w"] = r.width();
        srcSize["h"] = r.height();
        frameData["sourceSize"] = srcSize;

        QPoint piv = doc.boxPivot(i);
        double normX = r.width() > 0 ? static_cast<double>(piv.x()) / r.width() : 0.5;
        double normY = r.height() > 0 ? static_cast<double>(piv.y()) / r.height() : 1.0;
        QJsonObject pivotObj;
        pivotObj["x"] = normX;
        pivotObj["y"] = normY;
        frameData["pivot"] = pivotObj;

        if (i < doc.frameCount()) {
            const SpriteBox &box = doc.box(i);
            if (box.hasPolygonMesh && !box.polygon.isEmpty() && !box.triangles.isEmpty()) {
                QJsonArray verticesArr;
                QJsonArray verticesUVArr;
                const double atlasW = packResult.dimensions.width();
                const double atlasH = packResult.dimensions.height();

                const QList<QPointF> meshVertices = !box.vertices.isEmpty() ? box.vertices :
                    (box.polygon.isClosed() && box.polygon.size() >= 4 ? box.polygon.mid(0, box.polygon.size() - 1).toList() : box.polygon.toList());

                for (const QPointF &pt : meshVertices) {
                    QJsonArray ptArr;
                    ptArr.append(pt.x());
                    ptArr.append(pt.y());
                    verticesArr.append(ptArr);

                    QJsonArray uvArr;
                    double u = (atlasW > 0.0) ? ((r.x() + pt.x()) / atlasW) : 0.0;
                    double v = (atlasH > 0.0) ? ((r.y() + pt.y()) / atlasH) : 0.0;
                    uvArr.append(u);
                    uvArr.append(v);
                    verticesUVArr.append(uvArr);
                }
                frameData["vertices"] = verticesArr;
                frameData["verticesUV"] = verticesUVArr;

                QJsonArray trianglesArr;
                for (int t = 0; t + 2 < box.triangles.size(); t += 3) {
                    QJsonArray tri;
                    tri.append(box.triangles[t]);
                    tri.append(box.triangles[t + 1]);
                    tri.append(box.triangles[t + 2]);
                    trianglesArr.append(tri);
                }
                frameData["triangles"] = trianglesArr;
            }
        }

        QString frameKey = QStringLiteral("%1_%2").arg(baseName).arg(i, 4, 10, QLatin1Char('0'));
        framesObj[frameKey] = frameData;
    }
    rootObj["frames"] = framesObj;

    // Meta object
    QJsonObject metaObj;
    metaObj["app"] = QStringLiteral("SpriteStudio");
    metaObj["version"] = version().toString();
    metaObj["image"] = pngFileName;
    metaObj["format"] = QStringLiteral("RGBA8888");

    QJsonObject sizeObj;
    sizeObj["w"] = packResult.dimensions.width();
    sizeObj["h"] = packResult.dimensions.height();
    metaObj["size"] = sizeObj;
    metaObj["scale"] = QStringLiteral("1");

    // Frame tags for animations
    QJsonArray frameTagsArray;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        QJsonObject tagObj;
        tagObj["name"] = it.key();
        if (!it.value().frameIndices.isEmpty()) {
            tagObj["from"] = it.value().frameIndices.first();
            tagObj["to"] = it.value().frameIndices.last();
        } else {
            tagObj["from"] = 0;
            tagObj["to"] = 0;
        }
        tagObj["direction"] = QStringLiteral("forward");
        tagObj["fps"] = it.value().fps;
        frameTagsArray.append(tagObj);
    }
    metaObj["frameTags"] = frameTagsArray;

    rootObj["meta"] = metaObj;

    QFile jsonOut(filePath);
    if (!jsonOut.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            error->code = ExtractorError::FileNotWritable;
            error->message = tr("Cannot write to JSON file: %1").arg(filePath);
            error->filePath = filePath;
        }
        return false;
    }

    QJsonDocument jsonDoc(rootObj);
    jsonOut.write(jsonDoc.toJson(QJsonDocument::Indented));
    jsonOut.close();

    setProgress(100);
    setStatusMessage(tr("Exported JSON descriptor %1 and image %2").arg(QFileInfo(filePath).fileName(), pngFileName));
    return true;
}
