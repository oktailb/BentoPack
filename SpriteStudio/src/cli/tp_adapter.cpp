#include "cli/tp_adapter.h"
#include "cli/godot_pipeline.h"
#include "controller/projectcontroller.h"
#include "extractor/extractorregistry.h"
#include "extractor/gifextractor.h"
#include "project/sessionmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QRegularExpression>

namespace SpriteStudioCli {

static QRect computeTrimmedRect(const QImage &img, int alphaThreshold)
{
    int w = img.width();
    int h = img.height();
    if (w == 0 || h == 0) return QRect(0, 0, 0, 0);

    int minX = w, maxX = -1, minY = h, maxY = -1;
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (qAlpha(line[x]) > alphaThreshold) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (minX > maxX || minY > maxY) {
        return QRect(0, 0, 1, 1);
    }
    return QRect(minX, minY, (maxX - minX) + 1, (maxY - minY) + 1);
}

bool TexturePackerAdapter::collectInputImages(const QStringList &inputPaths,
                                             QList<QImage> &outFrames,
                                             QList<QString> &outNames,
                                             QList<QPoint> &outPivots,
                                             QMap<QString, SpriteAnimation> &outAnimations,
                                             bool prependFolderName,
                                             QString *outError)
{
    for (const QString &pathStr : inputPaths) {
        QFileInfo fi(pathStr);
        if (!fi.exists()) {
            if (outError) *outError = QStringLiteral("Input path not found: ") + pathStr;
            return false;
        }

        if (fi.isDir()) {
            QDirIterator it(fi.absoluteFilePath(),
                            { QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.bmp"), QStringLiteral("*.webp") },
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString filePath = it.next();
                QImage img(filePath);
                if (!img.isNull()) {
                    QFileInfo subFi(filePath);
                    QString name = prependFolderName
                        ? (subFi.dir().dirName() + QStringLiteral("/") + subFi.completeBaseName())
                        : subFi.completeBaseName();
                    outFrames.append(img.convertToFormat(QImage::Format_ARGB32_Premultiplied));
                    outNames.append(name);
                    outPivots.append(QPoint(img.width() / 2, img.height() / 2));
                }
            }
        } else {
            QString ext = fi.suffix().toLower();
            if (ext == QStringLiteral("ssp") || ext == QStringLiteral("json") ||
                ext == QStringLiteral("tres") || ext == QStringLiteral("gif")) {
                SpriteDocument doc;
                ProjectController controller(&doc);
                QString err;
                if (!controller.openFile(fi.absoluteFilePath(), &err)) {
                    if (outError) *outError = QStringLiteral("Failed to load file: ") + fi.absoluteFilePath() + QStringLiteral(" (") + err + QStringLiteral(")");
                    return false;
                }
                for (int i = 0; i < doc.frameCount(); ++i) {
                    outFrames.append(doc.frame(i));
                    outNames.append(QStringLiteral("frame_%1").arg(i));
                    outPivots.append(doc.boxPivot(i));
                }
                for (auto ait = doc.animations().begin(); ait != doc.animations().end(); ++ait) {
                    outAnimations.insert(ait.key(), ait.value());
                }
            } else {
                QImage img(fi.absoluteFilePath());
                if (!img.isNull()) {
                    outFrames.append(img.convertToFormat(QImage::Format_ARGB32_Premultiplied));
                    outNames.append(fi.completeBaseName());
                    outPivots.append(QPoint(img.width() / 2, img.height() / 2));
                } else {
                    if (outError) *outError = QStringLiteral("Failed to read image file: ") + fi.absoluteFilePath();
                    return false;
                }
            }
        }
    }

    return true;
}

bool TexturePackerAdapter::writeTexturePackerJson(const QString &jsonPath,
                                                 const QString &relImagePath,
                                                 const QList<QRect> &packedRects,
                                                 const QList<QString> &frameNames,
                                                 const QList<QImage> &originalFrames,
                                                 const QList<QPoint> &pivots,
                                                 const QSize &atlasSize,
                                                 bool jsonArrayFormat,
                                                 QString *outError)
{
    QFileInfo fi(jsonPath);
    QDir().mkpath(fi.dir().absolutePath());

    QJsonObject rootObj;
    QJsonArray framesArray;
    QJsonObject framesHash;

    for (int i = 0; i < packedRects.size(); ++i) {
        const QRect &r = packedRects[i];
        const QImage &orig = (i < originalFrames.size()) ? originalFrames[i] : QImage();
        QPoint piv = (i < pivots.size()) ? pivots[i] : QPoint(r.width() / 2, r.height() / 2);
        QString name = (i < frameNames.size()) ? frameNames[i] : QStringLiteral("frame_%1").arg(i);
        if (!name.endsWith(QStringLiteral(".png"))) {
            name += QStringLiteral(".png");
        }

        QJsonObject fObj;
        if (jsonArrayFormat) {
            fObj[QStringLiteral("filename")] = name;
        }

        // Frame in atlas
        QJsonObject frameRect;
        frameRect[QStringLiteral("x")] = r.x();
        frameRect[QStringLiteral("y")] = r.y();
        frameRect[QStringLiteral("w")] = r.width();
        frameRect[QStringLiteral("h")] = r.height();
        fObj[QStringLiteral("frame")] = frameRect;

        fObj[QStringLiteral("rotated")] = false;
        bool isTrimmed = (orig.width() > r.width() || orig.height() > r.height());
        fObj[QStringLiteral("trimmed")] = isTrimmed;

        // Sprite Source Size
        QJsonObject sss;
        sss[QStringLiteral("x")] = 0;
        sss[QStringLiteral("y")] = 0;
        sss[QStringLiteral("w")] = r.width();
        sss[QStringLiteral("h")] = r.height();
        fObj[QStringLiteral("spriteSourceSize")] = sss;

        // Source Size
        QJsonObject srcSize;
        srcSize[QStringLiteral("w")] = orig.isNull() ? r.width() : orig.width();
        srcSize[QStringLiteral("h")] = orig.isNull() ? r.height() : orig.height();
        fObj[QStringLiteral("sourceSize")] = srcSize;

        // Pivot normalized [0.0, 1.0]
        QJsonObject pivotObj;
        double origW = orig.isNull() ? r.width() : orig.width();
        double origH = orig.isNull() ? r.height() : orig.height();
        pivotObj[QStringLiteral("x")] = (origW > 0) ? (static_cast<double>(piv.x()) / origW) : 0.5;
        pivotObj[QStringLiteral("y")] = (origH > 0) ? (static_cast<double>(piv.y()) / origH) : 0.5;
        fObj[QStringLiteral("pivot")] = pivotObj;

        if (jsonArrayFormat) {
            framesArray.append(fObj);
        } else {
            framesHash[name] = fObj;
        }
    }

    if (jsonArrayFormat) {
        rootObj[QStringLiteral("frames")] = framesArray;
    } else {
        rootObj[QStringLiteral("frames")] = framesHash;
    }

    // Meta object
    QJsonObject meta;
    meta[QStringLiteral("app")] = QStringLiteral("SpriteStudio");
    meta[QStringLiteral("version")] = QStringLiteral("1.0.0");
    meta[QStringLiteral("image")] = relImagePath;
    meta[QStringLiteral("format")] = QStringLiteral("RGBA8888");
    QJsonObject sizeObj;
    sizeObj[QStringLiteral("w")] = atlasSize.width();
    sizeObj[QStringLiteral("h")] = atlasSize.height();
    meta[QStringLiteral("size")] = sizeObj;
    meta[QStringLiteral("scale")] = QStringLiteral("1");
    rootObj[QStringLiteral("meta")] = meta;

    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) *outError = QStringLiteral("Cannot write JSON file: ") + jsonPath;
        return false;
    }

    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

CliResult TexturePackerAdapter::execute(const QStringList &args)
{
    QString sheetPath;
    QString dataPath;
    QString format = QStringLiteral("json-array");
    QString algorithm = QStringLiteral("MaxRects");
    QString heuristic = QStringLiteral("BestShortSideFit");
    bool sizePot = false;
    int maxWidth = 4096;
    int maxHeight = 4096;
    int padding = 2;
    int borderPadding = 0;
    int extrude = 0;
    QString trimMode = QStringLiteral("None");
    int trimThreshold = 1;
    bool autoAlias = true;
    bool hasCustomPivot = false;
    double pivotX = 0.5, pivotY = 0.5;
    bool prependFolder = false;
    QString godotUid;
    QString godotScene;
    QStringList inputPaths;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--sheet") && i + 1 < args.size()) {
            sheetPath = args[++i];
        } else if (arg == QStringLiteral("--data") && i + 1 < args.size()) {
            dataPath = args[++i];
        } else if (arg == QStringLiteral("--format") && i + 1 < args.size()) {
            format = args[++i].toLower();
        } else if (arg == QStringLiteral("--algorithm") && i + 1 < args.size()) {
            algorithm = args[++i];
        } else if (arg == QStringLiteral("--maxrects-heuristics") && i + 1 < args.size()) {
            heuristic = args[++i];
        } else if (arg == QStringLiteral("--size-constraints") && i + 1 < args.size()) {
            QString c = args[++i].toUpper();
            if (c == QStringLiteral("POT")) sizePot = true;
        } else if (arg == QStringLiteral("--max-size") && i + 1 < args.size()) {
            maxWidth = args[++i].toInt();
            if (i + 1 < args.size() && !args[i + 1].startsWith(QStringLiteral("-"))) {
                maxHeight = args[++i].toInt();
            } else {
                maxHeight = maxWidth;
            }
        } else if (arg == QStringLiteral("--max-width") && i + 1 < args.size()) {
            maxWidth = args[++i].toInt();
        } else if (arg == QStringLiteral("--max-height") && i + 1 < args.size()) {
            maxHeight = args[++i].toInt();
        } else if (arg == QStringLiteral("--padding") && i + 1 < args.size()) {
            padding = args[++i].toInt();
        } else if (arg == QStringLiteral("--shape-padding") && i + 1 < args.size()) {
            padding = args[++i].toInt();
        } else if (arg == QStringLiteral("--border-padding") && i + 1 < args.size()) {
            borderPadding = args[++i].toInt();
        } else if (arg == QStringLiteral("--extrude") && i + 1 < args.size()) {
            extrude = args[++i].toInt();
        } else if (arg == QStringLiteral("--trim-mode") && i + 1 < args.size()) {
            trimMode = args[++i];
        } else if (arg == QStringLiteral("--trim-threshold") && i + 1 < args.size()) {
            trimThreshold = args[++i].toInt();
        } else if (arg == QStringLiteral("--enable-auto-alias") || arg == QStringLiteral("--detect-identical-sprites")) {
            autoAlias = true;
        } else if (arg == QStringLiteral("--disable-auto-alias")) {
            autoAlias = false;
        } else if (arg == QStringLiteral("--pivot-point") && i + 2 < args.size()) {
            pivotX = args[++i].toDouble();
            pivotY = args[++i].toDouble();
            hasCustomPivot = true;
        } else if (arg == QStringLiteral("--prepend-folder-name")) {
            prependFolder = true;
        } else if (arg == QStringLiteral("--godot-uid") && i + 1 < args.size()) {
            godotUid = args[++i];
        } else if (arg == QStringLiteral("--godot-scene") && i + 1 < args.size()) {
            godotScene = args[++i];
        } else if (!arg.startsWith(QStringLiteral("-"))) {
            inputPaths.append(arg);
        }
    }

    if (inputPaths.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No input files or directories specified."));
    }

    if (sheetPath.isEmpty() && dataPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("Must specify at least --sheet or --data."));
    }

    if (sheetPath.isEmpty() && !dataPath.isEmpty()) {
        QFileInfo dfi(dataPath);
        sheetPath = dfi.dir().filePath(dfi.completeBaseName() + QStringLiteral(".png"));
    }

    // 1. Gather input frames
    QList<QImage> rawFrames;
    QList<QString> frameNames;
    QList<QPoint> pivots;
    QMap<QString, SpriteAnimation> animations;
    QString gatherErr;
    if (!collectInputImages(inputPaths, rawFrames, frameNames, pivots, animations, prependFolder, &gatherErr)) {
        return CliResult::error(ExitFileNotFound, gatherErr);
    }

    if (rawFrames.isEmpty()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("No valid image frames found in input paths."));
    }

    // 2. Process pivots if custom pivot specified
    if (hasCustomPivot) {
        for (int i = 0; i < rawFrames.size(); ++i) {
            int px = qRound(rawFrames[i].width() * pivotX);
            int py = qRound(rawFrames[i].height() * pivotY);
            if (i < pivots.size()) {
                pivots[i] = QPoint(px, py);
            } else {
                pivots.append(QPoint(px, py));
            }
        }
    }

    // 3. Process trimming if requested
    QList<QImage> packFrames;
    packFrames.reserve(rawFrames.size());
    bool doTrim = (trimMode.compare(QStringLiteral("Trim"), Qt::CaseInsensitive) == 0 ||
                   trimMode.compare(QStringLiteral("Crop"), Qt::CaseInsensitive) == 0);
    for (int i = 0; i < rawFrames.size(); ++i) {
        if (doTrim) {
            QRect trimRect = computeTrimmedRect(rawFrames[i], trimThreshold);
            packFrames.append(rawFrames[i].copy(trimRect));
            // Adjust pivot relative to trimmed rect
            if (i < pivots.size()) {
                pivots[i] = pivots[i] - trimRect.topLeft();
            }
        } else {
            packFrames.append(rawFrames[i]);
        }
    }

    // 4. Configure AtlasPacker
    AtlasPacker::PackOptions packOpts;
    if (algorithm.compare(QStringLiteral("Basic"), Qt::CaseInsensitive) == 0) {
        packOpts.algorithm = AtlasPacker::RowPacker;
    } else if (algorithm.compare(QStringLiteral("Grid"), Qt::CaseInsensitive) == 0) {
        packOpts.algorithm = AtlasPacker::GridPacker;
    } else {
        packOpts.algorithm = AtlasPacker::MaxRects;
    }

    if (heuristic.compare(QStringLiteral("BestLongSideFit"), Qt::CaseInsensitive) == 0) {
        packOpts.heuristic = MaxRectsHeuristic::BestLongSideFit;
    } else if (heuristic.compare(QStringLiteral("BestAreaFit"), Qt::CaseInsensitive) == 0) {
        packOpts.heuristic = MaxRectsHeuristic::BestAreaFit;
    } else if (heuristic.compare(QStringLiteral("BottomLeft"), Qt::CaseInsensitive) == 0) {
        packOpts.heuristic = MaxRectsHeuristic::BottomLeft;
    } else if (heuristic.compare(QStringLiteral("ContactPoint"), Qt::CaseInsensitive) == 0) {
        packOpts.heuristic = MaxRectsHeuristic::ContactPoint;
    } else {
        packOpts.heuristic = MaxRectsHeuristic::BestShortSideFit;
    }

    packOpts.padding = padding;
    packOpts.borderPadding = borderPadding;
    packOpts.extrude = extrude;
    packOpts.powerOfTwo = sizePot;
    packOpts.maxWidth = maxWidth;
    packOpts.maxHeight = maxHeight;
    packOpts.deduplicate = autoAlias;

    QElapsedTimer timer;
    timer.start();
    AtlasPackResult packRes = AtlasPacker::pack(packFrames, packOpts);
    qint64 elapsedMs = timer.elapsed();

    if (!packRes.success) {
        return CliResult::error(ExitConstraintFailed,
                                QStringLiteral("Packing failed: frames exceed max-size constraints (%1x%2).")
                                .arg(maxWidth).arg(maxHeight));
    }

    // 5. Export metadata
    bool isGodot = (format == QStringLiteral("godot") || format == QStringLiteral("godot4") ||
                    dataPath.endsWith(QStringLiteral(".tres"), Qt::CaseInsensitive));

    if (isGodot) {
        GodotPipeline::ExportArgs gArgs;
        gArgs.sheetPath = sheetPath;
        gArgs.tresPath = dataPath.isEmpty() ? (QFileInfo(sheetPath).path() + "/" + QFileInfo(sheetPath).completeBaseName() + ".tres") : dataPath;
        gArgs.scenePath = godotScene;
        gArgs.explicitUid = godotUid;
        gArgs.preserveExistingUid = true;
        gArgs.frames = packFrames;
        gArgs.frameRects = packRes.frameRects;
        gArgs.pivots = pivots;
        gArgs.animations = animations;
        gArgs.atlas = packRes.atlas;

        CliResult gRes = GodotPipeline::exportGodot(gArgs);
        if (gRes.exitCode != ExitSuccess) {
            return gRes;
        }
    } else {
        // Save Sheet image
        QFileInfo sfi(sheetPath);
        QDir().mkpath(sfi.dir().absolutePath());
        if (!packRes.atlas.save(sheetPath, "PNG")) {
            return CliResult::error(ExitIoError, QStringLiteral("Failed to write atlas sheet image: ") + sheetPath);
        }

        // Save JSON data
        if (!dataPath.isEmpty()) {
            QString jsonErr;
            QString relImg = QFileInfo(sheetPath).fileName();
            bool isArray = (format != QStringLiteral("json-hash"));
            if (!writeTexturePackerJson(dataPath, relImg, packRes.frameRects, frameNames, rawFrames, pivots, packRes.dimensions, isArray, &jsonErr)) {
                return CliResult::error(ExitIoError, jsonErr);
            }
        }
    }

    QJsonObject json;
    json[QStringLiteral("atlas")] = sheetPath;
    if (!dataPath.isEmpty()) json[QStringLiteral("data")] = dataPath;
    json[QStringLiteral("width")] = packRes.dimensions.width();
    json[QStringLiteral("height")] = packRes.dimensions.height();
    json[QStringLiteral("frames_count")] = packRes.frameRects.size();
    json[QStringLiteral("unique_frames")] = packRes.uniqueFramesCount;
    json[QStringLiteral("efficiency")] = static_cast<double>(packRes.efficiency);
    json[QStringLiteral("elapsed_ms")] = elapsedMs;

    QString summary = QStringLiteral("Packed %1 frames into %2x%3 atlas (%4% efficiency) in %5 ms.")
        .arg(packRes.frameRects.size())
        .arg(packRes.dimensions.width())
        .arg(packRes.dimensions.height())
        .arg(QString::number(packRes.efficiency, 'f', 1))
        .arg(elapsedMs);

    return CliResult::success(summary, json);
}

} // namespace SpriteStudioCli
