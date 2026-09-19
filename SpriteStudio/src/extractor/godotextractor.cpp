#include "extractor/godotextractor.h"
#include "packer/atlaspacker.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
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
            QRegularExpression triRegex(QStringLiteral(R"re(metadata/frame_(\d+)/triangles\s*=\s*PackedInt32Array\(([^)]*)\))re"));

            QRegularExpressionMatchIterator pIter = polyRegex.globalMatch(meshContent);
            while (pIter.hasNext()) {
                QRegularExpressionMatch match = pIter.next();
                int idx = match.captured(1).toInt();
                QString coordsStr = match.captured(2).trimmed();
                if (idx >= 0 && idx < boxes.size() && !coordsStr.isEmpty()) {
                    QStringList parts = coordsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    QPolygonF poly;
                    QList<QPointF> verts;
                    for (int p = 0; p + 1 < parts.size(); p += 2) {
                        double x = parts[p].trimmed().toDouble();
                        double y = parts[p + 1].trimmed().toDouble();
                        poly.append(QPointF(x, y));
                        verts.append(QPointF(x, y));
                    }
                    if (!poly.isEmpty()) {
                        boxes[idx].hasPolygonMesh = true;
                        boxes[idx].polygon = poly;
                        boxes[idx].vertices = verts;
                    }
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
                    boxes[idx].triangles = tris;
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
    QString imagePath = dir.filePath(imageFilename);
    QString tresPath = dir.filePath(baseName + ".tres");

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
            error->message = tr("Failed to pack atlas frames for Godot export.");
            error->filePath = filePath;
        }
        return false;
    }

    setProgress(60);

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

    // Write Godot 4.x SpriteFrames header
    out << "[gd_resource type=\"SpriteFrames\" load_steps=" << (packResult.frameRects.size() + 2) << " format=3]\n\n";
    out << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";

    for (int i = 0; i < packResult.frameRects.size(); ++i) {
        const QRect &r = packResult.frameRects[i];
        QString subResId = QString("AtlasTexture_%1").arg(i);
        out << "[sub_resource type=\"AtlasTexture\" id=\"" << subResId << "\"]\n";
        out << "atlas = ExtResource(\"1_atlas\")\n";
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

    // If any frame has a tight polygon mesh, write companion _mesh.tres resource
    bool hasMesh = false;
    for (int i = 0; i < doc.frameCount(); ++i) {
        if (doc.box(i).hasPolygonMesh && !doc.box(i).polygon.isEmpty() && !doc.box(i).triangles.isEmpty()) {
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
            mOut << "[ext_resource type=\"Texture2D\" path=\"res://" << imageFilename << "\" id=\"1_atlas\"]\n\n";
            mOut << "[resource]\n";
            mOut << "metadata/texture = ExtResource(\"1_atlas\")\n";
            mOut << "metadata/atlas_width = " << packResult.dimensions.width() << "\n";
            mOut << "metadata/atlas_height = " << packResult.dimensions.height() << "\n";

            const double aW = packResult.dimensions.width();
            const double aH = packResult.dimensions.height();

            for (int i = 0; i < packResult.frameRects.size(); ++i) {
                const QRect &r = packResult.frameRects[i];
                if (i < doc.frameCount()) {
                    const SpriteBox &b = doc.box(i);
                    if (b.hasPolygonMesh && !b.polygon.isEmpty() && !b.triangles.isEmpty()) {
                        mOut << "metadata/frame_" << i << "/rect = Rect2(" << r.x() << ", " << r.y() << ", " << r.width() << ", " << r.height() << ")\n";

                        const QList<QPointF> meshVertices = !b.vertices.isEmpty() ? b.vertices :
                            (b.polygon.isClosed() && b.polygon.size() >= 4 ? b.polygon.mid(0, b.polygon.size() - 1).toList() : b.polygon.toList());

                        // Polygon coordinates
                        mOut << "metadata/frame_" << i << "/polygon = PackedVector2Array(";
                        for (int p = 0; p < meshVertices.size(); ++p) {
                            if (p > 0) mOut << ", ";
                            mOut << meshVertices[p].x() << ", " << meshVertices[p].y();
                        }
                        mOut << ")\n";

                        // UV coordinates
                        mOut << "metadata/frame_" << i << "/uv = PackedVector2Array(";
                        for (int p = 0; p < meshVertices.size(); ++p) {
                            if (p > 0) mOut << ", ";
                            double u = (aW > 0.0) ? ((r.x() + meshVertices[p].x()) / aW) : 0.0;
                            double v = (aH > 0.0) ? ((r.y() + meshVertices[p].y()) / aH) : 0.0;
                            mOut << u << ", " << v;
                        }
                        mOut << ")\n";

                        // Triangles
                        mOut << "metadata/frame_" << i << "/triangles = PackedInt32Array(";
                        for (int t = 0; t < b.triangles.size(); ++t) {
                            if (t > 0) mOut << ", ";
                            mOut << b.triangles[t];
                        }
                        mOut << ")\n";
                    }
                }
            }
            meshFile.close();
        }
    }

    setProgress(100);
    setStatusMessage(tr("Exported Godot resource: %1 and image %2").arg(QFileInfo(tresPath).fileName(), imageFilename));
    return true;
}
