/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "cli/clipackpipeline.h"
#include "controller/projectcontroller.h"
#include "extractor/extractorregistry.h"
#include "packer/vramtexturecompressor.h"
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

bool CliPackPipeline::collectInputImages(const QStringList &inputPaths,
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
                }
            }
        }
    }

    if (outFrames.isEmpty()) {
        if (outError) *outError = QStringLiteral("No valid image files found in specified paths.");
        return false;
    }

    return true;
}

CliResult CliPackPipeline::execute(const QStringList &args)
{
    QString sheetPath;
    QString dataPath;
    QString format;
    QString algorithm = QStringLiteral("MaxRects");
    QString heuristic = QStringLiteral("BestShortSideFit");
    bool sizePot = false;
    int maxWidth = 4096;
    int maxHeight = 4096;
    int padding = 2;
    int borderPadding = 0;
    int extrude = 0;
    QString trimMode = QStringLiteral("Trim");
    int trimThreshold = 1;
    bool autoAlias = true;
    double pivotX = 0.5;
    double pivotY = 0.5;
    bool hasCustomPivot = false;
    bool prependFolder = false;
    QString godotUid;
    QString godotScene;
    bool listTags = false;

    // VRAM options
    QString textureFormat = QStringLiteral("png");
    QString vramFormatStr = QStringLiteral("uastc");
    int vramQuality = 128;
    bool zstd = true;
    int zstdLevel = 9;

    QStringList inputPaths;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];

        if (arg == QStringLiteral("--sheet") && i + 1 < args.size()) {
            sheetPath = args[++i];
        } else if (arg == QStringLiteral("--data") && i + 1 < args.size()) {
            dataPath = args[++i];
        } else if (arg == QStringLiteral("--format") && i + 1 < args.size()) {
            format = args[++i];
        } else if (arg == QStringLiteral("--texture-format") && i + 1 < args.size()) {
            textureFormat = args[++i].toLower();
        } else if (arg == QStringLiteral("--vram-format") && i + 1 < args.size()) {
            vramFormatStr = args[++i].toLower();
        } else if (arg == QStringLiteral("--vram-quality") && i + 1 < args.size()) {
            vramQuality = args[++i].toInt();
        } else if (arg == QStringLiteral("--opt") && i + 1 < args.size()) {
            ++i; // consume --opt value
        } else if (arg == QStringLiteral("--enable-zstd")) {
            zstd = true;
        } else if (arg == QStringLiteral("--disable-zstd")) {
            zstd = false;
        } else if (arg == QStringLiteral("--zstd-level") && i + 1 < args.size()) {
            zstdLevel = args[++i].toInt();
        } else if (arg == QStringLiteral("--algorithm") && i + 1 < args.size()) {
            algorithm = args[++i];
        } else if (arg == QStringLiteral("--maxrects-heuristics") && i + 1 < args.size()) {
            heuristic = args[++i];
        } else if (arg == QStringLiteral("--size-constraints") && i + 1 < args.size()) {
            sizePot = (args[++i].compare(QStringLiteral("POT"), Qt::CaseInsensitive) == 0);
        } else if (arg == QStringLiteral("--max-size") && i + 2 < args.size()) {
            maxWidth = args[++i].toInt();
            maxHeight = args[++i].toInt();
        } else if (arg == QStringLiteral("--max-width") && i + 1 < args.size()) {
            maxWidth = args[++i].toInt();
        } else if (arg == QStringLiteral("--max-height") && i + 1 < args.size()) {
            maxHeight = args[++i].toInt();
        } else if ((arg == QStringLiteral("--padding") || arg == QStringLiteral("--inner-padding") || arg == QStringLiteral("--shape-padding")) && i + 1 < args.size()) {
            padding = args[++i].toInt();
        } else if (arg == QStringLiteral("--border-padding") && i + 1 < args.size()) {
            borderPadding = args[++i].toInt();
        } else if (arg == QStringLiteral("--sheet-type") && i + 1 < args.size()) {
            ++i; // consume --sheet-type value
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
        } else if (arg == QStringLiteral("--ignore-empty")) {
            // flag
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
        } else if (arg == QStringLiteral("--list-tags")) {
            listTags = true;
        } else if (arg == QStringLiteral("-b") || arg == QStringLiteral("--batch")) {
            // aseprite batch flag, consume
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

    // 5. Setup VRAM options
    bool isVram = (textureFormat == QStringLiteral("ktx2") || textureFormat == QStringLiteral("basis") ||
                   sheetPath.endsWith(QStringLiteral(".ktx2"), Qt::CaseInsensitive) ||
                   sheetPath.endsWith(QStringLiteral(".basis"), Qt::CaseInsensitive));
    if (isVram && !sheetPath.endsWith(QStringLiteral(".ktx2"), Qt::CaseInsensitive) && !sheetPath.endsWith(QStringLiteral(".basis"), Qt::CaseInsensitive)) {
        if (sheetPath.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
            sheetPath.chop(4);
        }
        sheetPath += (textureFormat == QStringLiteral("basis")) ? QStringLiteral(".basis") : QStringLiteral(".ktx2");
    }

    VramCompressionOptions vOpts;
    if (sheetPath.endsWith(QStringLiteral(".basis"), Qt::CaseInsensitive)) {
        vOpts.format = (vramFormatStr == QStringLiteral("etc1s")) ? VramFormat::Basis_ETC1S : VramFormat::Basis_UASTC;
    } else {
        vOpts.format = (vramFormatStr == QStringLiteral("etc1s")) ? VramFormat::KTX2_ETC1S : VramFormat::KTX2_UASTC;
    }
    vOpts.qualityLevel = vramQuality;
    vOpts.zstdSupercompression = zstd;
    vOpts.zstdLevel = zstdLevel;
    VramCompressionStats vStats;

    // 6. Save sheet image
    QFileInfo sfi(sheetPath);
    QDir().mkpath(sfi.dir().absolutePath());
    if (isVram) {
        QString vErr;
        if (!VramTextureCompressor::compressToFile(packRes.atlas, sheetPath, vOpts, &vStats, &vErr)) {
            return CliResult::error(ExitIoError, QStringLiteral("Failed to compress VRAM atlas: ") + sheetPath + QStringLiteral(" (") + vErr + QStringLiteral(")"));
        }
    } else {
        if (!packRes.atlas.save(sheetPath, "PNG")) {
            return CliResult::error(ExitIoError, QStringLiteral("Failed to write atlas sheet image: ") + sheetPath);
        }
    }

    // 7. Save metadata via ExtractorRegistry plugin lookup
    if (!dataPath.isEmpty()) {
        SpriteDocument doc;
        doc.setAtlas(packRes.atlas);
        QList<SpriteBox> boxes;
        boxes.reserve(packFrames.size());
        for (int i = 0; i < packFrames.size(); ++i) {
            SpriteBox b;
            if (i < packRes.frameRects.size()) {
                b.rect = packRes.frameRects[i];
            } else {
                b.rect = QRect(0, 0, packFrames[i].width(), packFrames[i].height());
            }
            b.pivot = (i < pivots.size()) ? pivots[i] : QPoint(b.rect.width() / 2, b.rect.height() / 2);
            boxes.append(b);
        }
        doc.setFrames(packFrames, boxes);
        for (auto it = animations.begin(); it != animations.end(); ++it) {
            doc.setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
        }

        ExportOptions expOpts;
        expOpts.packOptions = packOpts;
        expOpts.vramOptions = vOpts;
        if (isVram) {
            if (sheetPath.endsWith(QStringLiteral(".basis"), Qt::CaseInsensitive)) {
                expOpts.textureFormat = TEXTURE_FORMAT_BASIS;
            } else {
                expOpts.textureFormat = (vOpts.format == VramFormat::KTX2_ETC1S) ? TEXTURE_FORMAT_KTX2_ETC1S : TEXTURE_FORMAT_KTX2_UASTC;
            }
        } else {
            expOpts.textureFormat = TEXTURE_FORMAT_PNG;
        }

        expOpts.extraParams[QStringLiteral("sheet_path")] = sheetPath;
        expOpts.extraParams[QStringLiteral("godot_uid")] = godotUid;
        expOpts.extraParams[QStringLiteral("godot_scene")] = godotScene;
        expOpts.extraParams[QStringLiteral("format")] = format;
        expOpts.extraParams[QStringLiteral("list_tags")] = listTags;
        if (format == QStringLiteral("json-array")) {
            expOpts.extraParams[QStringLiteral("json_format")] = QStringLiteral("array");
        } else if (format == QStringLiteral("json-hash")) {
            expOpts.extraParams[QStringLiteral("json_format")] = QStringLiteral("hash");
        }

        Extractor *encoder = nullptr;
        if (!format.isEmpty()) {
            encoder = ExtractorRegistry::instance().findExtractorById(format);
            if (!encoder) {
                encoder = ExtractorRegistry::instance().findEncoderByFilter(format);
            }
        }
        if (!encoder) {
            encoder = ExtractorRegistry::instance().findEncoder(dataPath);
        }

        if (!encoder) {
            return CliResult::error(ExitPluginNotFound,
                QStringLiteral("No extractor plugin found to export metadata to '%1'. Ensure appropriate plugin is installed and loaded.")
                .arg(dataPath));
        }

        ExtractorError expErr;
        if (!encoder->write(dataPath, doc, expOpts, &expErr)) {
            return CliResult::error(ExitIoError,
                QStringLiteral("Failed to export metadata (%1): %2")
                .arg(encoder->displayName(), expErr.toString()));
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
    if (isVram) {
        json[QStringLiteral("vram_format")] = VramTextureCompressor::formatName(vOpts.format);
        json[QStringLiteral("vram_bytes")] = vStats.compressedBytes;
        json[QStringLiteral("vram_savings_percent")] = vStats.vramSavingsPercent;
    }

    QString summary = QStringLiteral("Packed %1 frames into %2x%3 atlas (%4% efficiency) in %5 ms.")
        .arg(packRes.frameRects.size())
        .arg(packRes.dimensions.width())
        .arg(packRes.dimensions.height())
        .arg(QString::number(packRes.efficiency, 'f', 1))
        .arg(elapsedMs);

    if (isVram) {
        summary += QStringLiteral(" [VRAM: %1, -%2% savings]")
            .arg(VramTextureCompressor::formatName(vOpts.format))
            .arg(QString::number(vStats.vramSavingsPercent, 'f', 1));
    }

    return CliResult::success(summary, json);
}

} // namespace SpriteStudioCli
