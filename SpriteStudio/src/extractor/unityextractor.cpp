#include "extractor/unityextractor.h"
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
    return filePath.endsWith(QStringLiteral(".unity.json"), Qt::CaseInsensitive);
}

bool UnityExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(outDoc);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->message = tr("Unity export format is write-only.");
    }
    return false;
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
    QString pngFileName = baseName + ".png";
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

    if (!packResult.atlas.save(pngFilePath, "PNG")) {
        if (error) {
            error->code = ExtractorError::WriteFailed;
            error->message = tr("Failed to save companion image: %1").arg(pngFilePath);
            error->filePath = pngFilePath;
        }
        return false;
    }

    setProgress(75);

    // Build Unity JSON descriptor
    QJsonObject rootObj;
    rootObj["generator"] = QStringLiteral("SpriteStudio");
    rootObj["version"] = QString(PROJECT_VERSION);
    rootObj["format"] = QStringLiteral("Unity2D_SpriteMesh");
    rootObj["texture"] = pngFileName;

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
