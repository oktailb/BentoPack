#include "cli/aseprite_adapter.h"
#include "cli/tp_adapter.h"
#include "packer/atlaspacker.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

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

bool AsepriteAdapter::writeAsepriteJson(const QString &jsonPath,
                                        const QString &relImagePath,
                                        const QList<QRect> &packedRects,
                                        const QList<QString> &frameNames,
                                        const QList<QImage> &originalFrames,
                                        const QMap<QString, SpriteAnimation> &animations,
                                        const QSize &atlasSize,
                                        bool jsonArrayFormat,
                                        bool includeTags,
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
        QString name = (i < frameNames.size()) ? frameNames[i] : QStringLiteral("frame_%1").arg(i);
        if (!name.endsWith(QStringLiteral(".png"))) {
            name += QStringLiteral(".png");
        }

        QJsonObject fObj;
        if (jsonArrayFormat) {
            fObj[QStringLiteral("filename")] = name;
        }

        QJsonObject frameRect;
        frameRect[QStringLiteral("x")] = r.x();
        frameRect[QStringLiteral("y")] = r.y();
        frameRect[QStringLiteral("w")] = r.width();
        frameRect[QStringLiteral("h")] = r.height();
        fObj[QStringLiteral("frame")] = frameRect;

        fObj[QStringLiteral("rotated")] = false;
        fObj[QStringLiteral("trimmed")] = (orig.width() > r.width() || orig.height() > r.height());

        QJsonObject sss;
        sss[QStringLiteral("x")] = 0;
        sss[QStringLiteral("y")] = 0;
        sss[QStringLiteral("w")] = r.width();
        sss[QStringLiteral("h")] = r.height();
        fObj[QStringLiteral("spriteSourceSize")] = sss;

        QJsonObject srcSize;
        srcSize[QStringLiteral("w")] = orig.isNull() ? r.width() : orig.width();
        srcSize[QStringLiteral("h")] = orig.isNull() ? r.height() : orig.height();
        fObj[QStringLiteral("sourceSize")] = srcSize;

        fObj[QStringLiteral("duration")] = 100; // Standard 100ms per frame

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

    if (includeTags && !animations.isEmpty()) {
        QJsonArray frameTags;
        for (auto it = animations.begin(); it != animations.end(); ++it) {
            const SpriteAnimation &anim = it.value();
            if (anim.frameIndices.isEmpty()) continue;
            int minIdx = anim.frameIndices.first();
            int maxIdx = anim.frameIndices.first();
            for (int idx : anim.frameIndices) {
                if (idx < minIdx) minIdx = idx;
                if (idx > maxIdx) maxIdx = idx;
            }

            QJsonObject tag;
            tag[QStringLiteral("name")] = it.key();
            tag[QStringLiteral("from")] = minIdx;
            tag[QStringLiteral("to")] = maxIdx;
            tag[QStringLiteral("direction")] = anim.loop ? QStringLiteral("forward") : QStringLiteral("pingpong");
            frameTags.append(tag);
        }
        meta[QStringLiteral("frameTags")] = frameTags;
    }

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

CliResult AsepriteAdapter::execute(const QStringList &args)
{
    QString sheetPath;
    QString dataPath;
    QString format = QStringLiteral("json-array");
    QString sheetType = QStringLiteral("packed");
    int maxWidth = 4096;
    int maxHeight = 4096;
    int padding = 2;
    int borderPadding = 0;
    int extrude = 0;
    bool trim = false;
    bool listTags = false;
    bool ignoreEmpty = false;
    QStringList inputPaths;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("-b") || arg == QStringLiteral("--batch")) {
            // Batch / headless flag (native to CLI)
        } else if (arg == QStringLiteral("--sheet") && i + 1 < args.size()) {
            sheetPath = args[++i];
        } else if (arg == QStringLiteral("--data") && i + 1 < args.size()) {
            dataPath = args[++i];
        } else if (arg == QStringLiteral("--format") && i + 1 < args.size()) {
            format = args[++i].toLower();
        } else if (arg == QStringLiteral("--sheet-type") && i + 1 < args.size()) {
            sheetType = args[++i].toLower();
        } else if (arg == QStringLiteral("--sheet-width") && i + 1 < args.size()) {
            maxWidth = args[++i].toInt();
        } else if (arg == QStringLiteral("--sheet-height") && i + 1 < args.size()) {
            maxHeight = args[++i].toInt();
        } else if (arg == QStringLiteral("--inner-padding") && i + 1 < args.size()) {
            padding = args[++i].toInt();
        } else if (arg == QStringLiteral("--shape-padding") && i + 1 < args.size()) {
            padding = args[++i].toInt();
        } else if (arg == QStringLiteral("--border-padding") && i + 1 < args.size()) {
            borderPadding = args[++i].toInt();
        } else if (arg == QStringLiteral("--extrude")) {
            extrude = 1;
        } else if (arg == QStringLiteral("--trim") || arg == QStringLiteral("--trim-sprite")) {
            trim = true;
        } else if (arg == QStringLiteral("--list-tags")) {
            listTags = true;
        } else if (arg == QStringLiteral("--ignore-empty")) {
            ignoreEmpty = true;
        } else if (!arg.startsWith(QStringLiteral("-"))) {
            inputPaths.append(arg);
        }
    }

    if (inputPaths.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No input files specified."));
    }

    if (sheetPath.isEmpty() && dataPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("Must specify at least --sheet or --data."));
    }

    if (sheetPath.isEmpty() && !dataPath.isEmpty()) {
        QFileInfo dfi(dataPath);
        sheetPath = dfi.dir().filePath(dfi.completeBaseName() + QStringLiteral(".png"));
    }

    // 1. Collect inputs
    QList<QImage> rawFrames;
    QList<QString> frameNames;
    QList<QPoint> pivots;
    QMap<QString, SpriteAnimation> animations;
    QString gatherErr;
    if (!TexturePackerAdapter::collectInputImages(inputPaths, rawFrames, frameNames, pivots, animations, false, &gatherErr)) {
        return CliResult::error(ExitFileNotFound, gatherErr);
    }

    if (rawFrames.isEmpty()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("No valid frames found."));
    }

    // 2. Filter empty frames if requested
    QList<QImage> validFrames;
    QList<QString> validNames;
    for (int i = 0; i < rawFrames.size(); ++i) {
        if (ignoreEmpty) {
            bool empty = true;
            for (int y = 0; y < rawFrames[i].height() && empty; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb*>(rawFrames[i].constScanLine(y));
                for (int x = 0; x < rawFrames[i].width(); ++x) {
                    if (qAlpha(line[x]) > 0) {
                        empty = false;
                        break;
                    }
                }
            }
            if (empty) continue;
        }
        if (trim) {
            QRect trimRect = computeTrimmedRect(rawFrames[i], 1);
            validFrames.append(rawFrames[i].copy(trimRect));
        } else {
            validFrames.append(rawFrames[i]);
        }
        validNames.append(frameNames[i]);
    }

    // 3. Configure AtlasPacker
    AtlasPacker::PackOptions packOpts;
    if (sheetType == QStringLiteral("horizontal") || sheetType == QStringLiteral("vertical")) {
        packOpts.algorithm = AtlasPacker::RowPacker;
    } else if (sheetType == QStringLiteral("matrix")) {
        packOpts.algorithm = AtlasPacker::GridPacker;
    } else {
        packOpts.algorithm = AtlasPacker::MaxRects;
    }

    packOpts.padding = padding;
    packOpts.borderPadding = borderPadding;
    packOpts.extrude = extrude;
    packOpts.maxWidth = maxWidth;
    packOpts.maxHeight = maxHeight;

    QElapsedTimer timer;
    timer.start();
    AtlasPackResult packRes = AtlasPacker::pack(validFrames, packOpts);
    qint64 elapsedMs = timer.elapsed();

    if (!packRes.success) {
        return CliResult::error(ExitConstraintFailed, QStringLiteral("Aseprite packing failed: sheet constraints exceeded."));
    }

    // 4. Save sheet image
    QFileInfo sfi(sheetPath);
    QDir().mkpath(sfi.dir().absolutePath());
    if (!packRes.atlas.save(sheetPath, "PNG")) {
        return CliResult::error(ExitIoError, QStringLiteral("Cannot write sheet image: ") + sheetPath);
    }

    // 5. Save JSON data
    if (!dataPath.isEmpty()) {
        QString jsonErr;
        QString relImg = QFileInfo(sheetPath).fileName();
        bool isArray = (format != QStringLiteral("json-hash"));
        if (!writeAsepriteJson(dataPath, relImg, packRes.frameRects, validNames, validFrames, animations, packRes.dimensions, isArray, listTags, &jsonErr)) {
            return CliResult::error(ExitIoError, jsonErr);
        }
    }

    QJsonObject json;
    json[QStringLiteral("sheet")] = sheetPath;
    if (!dataPath.isEmpty()) json[QStringLiteral("data")] = dataPath;
    json[QStringLiteral("width")] = packRes.dimensions.width();
    json[QStringLiteral("height")] = packRes.dimensions.height();
    json[QStringLiteral("frames_count")] = packRes.frameRects.size();
    json[QStringLiteral("elapsed_ms")] = elapsedMs;

    QString summary = QStringLiteral("Aseprite batch: %1 frames packed to %2x%3 in %4 ms.")
        .arg(packRes.frameRects.size())
        .arg(packRes.dimensions.width())
        .arg(packRes.dimensions.height())
        .arg(elapsedMs);

    return CliResult::success(summary, json);
}

} // namespace SpriteStudioCli
