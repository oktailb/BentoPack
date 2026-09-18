#include "extractor/unrealextractor.h"
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
    return filePath.endsWith(QStringLiteral(".paper2d.json"), Qt::CaseInsensitive);
}

bool UnrealExtractor::read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(outDoc);
    if (error) {
        error->code = ExtractorError::UnsupportedFormat;
        error->message = tr("Unreal Paper2D export format is write-only.");
    }
    return false;
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
            error->message = tr("Failed to pack frames for Unreal Paper2D export.");
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
