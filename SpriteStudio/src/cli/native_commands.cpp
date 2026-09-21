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

#include "cli/native_commands.h"
#include "cli/clipackpipeline.h"
#include "image/spritedetector.h"
#include "project/sessionmanager.h"
#include "controller/projectcontroller.h"
#include "filters/filterplugin.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

namespace SpriteStudioCli {

CliResult NativeCommands::executePack(const QStringList &args)
{
    return CliPackPipeline::execute(args);
}

CliResult NativeCommands::executeSlice(const QStringList &args)
{
    QString inputPath;
    QString outputDir;
    QString outputProject;
    bool removeBg = false;
    int tolerance = 10;
    int alphaThreshold = 1;
    bool smartCrop = false;
    int minSliceSize = 3;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--output-dir") && i + 1 < args.size()) {
            outputDir = args[++i];
        } else if (arg == QStringLiteral("--output-project") && i + 1 < args.size()) {
            outputProject = args[++i];
        } else if (arg == QStringLiteral("--remove-bg")) {
            removeBg = true;
            if (i + 1 < args.size() && !args[i + 1].startsWith(QStringLiteral("-"))) {
                ++i; // optional color parameter consumed
            }
        } else if (arg == QStringLiteral("--tolerance") && i + 1 < args.size()) {
            tolerance = args[++i].toInt();
        } else if (arg == QStringLiteral("--alpha-threshold") && i + 1 < args.size()) {
            alphaThreshold = args[++i].toInt();
        } else if (arg == QStringLiteral("--smart-crop")) {
            smartCrop = true;
        } else if (arg == QStringLiteral("--min-size") && i + 1 < args.size()) {
            minSliceSize = args[++i].toInt();
        } else if (!arg.startsWith(QStringLiteral("-"))) {
            inputPath = arg;
        }
    }

    if (inputPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No input image specified for slice command."));
    }

    QFileInfo fi(inputPath);
    if (!fi.exists()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Input file not found: ") + inputPath);
    }

    QImage sourceImg(inputPath);
    if (sourceImg.isNull()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Cannot decode input image: ") + inputPath);
    }
    sourceImg = sourceImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QElapsedTimer timer;
    timer.start();

    // Background removal if requested
    if (removeBg) {
        sourceImg = ProjectController::removeBackgroundFromImage(sourceImg, tolerance);
    }

    SpriteDetectionOptions opts;
    opts.alphaThreshold = alphaThreshold;
    opts.smartCrop = smartCrop;
    opts.minSliceSize = minSliceSize;

    QList<QImage> outFrames;
    QList<SpriteBox> outBoxes;
    if (!SpriteDetector::detectToImages(sourceImg, outFrames, outBoxes, opts)) {
        return CliResult::error(ExitConstraintFailed, QStringLiteral("Sprite detection failed."));
    }
    qint64 elapsedMs = timer.elapsed();

    // Save slices to directory
    if (!outputDir.isEmpty()) {
        QDir().mkpath(outputDir);
        for (int i = 0; i < outFrames.size(); ++i) {
            QString slicePath = QDir(outputDir).filePath(QStringLiteral("slice_%1.png").arg(i));
            outFrames[i].save(slicePath, "PNG");
        }
    }

    // Save project .ssp
    if (!outputProject.isEmpty()) {
        SpriteDocument doc;
        doc.setFilePath(outputProject);
        doc.setAtlas(sourceImg);
        doc.setFrames(outFrames, outBoxes);
        ProjectController controller(&doc);
        QString err;
        if (!controller.saveProject(outputProject, &err)) {
            return CliResult::error(ExitIoError, QStringLiteral("Failed to save .ssp project: ") + err);
        }
    }

    QJsonObject json;
    json[QStringLiteral("input")] = inputPath;
    json[QStringLiteral("frames_detected")] = outFrames.size();
    if (!outputDir.isEmpty()) json[QStringLiteral("output_dir")] = outputDir;
    if (!outputProject.isEmpty()) json[QStringLiteral("output_project")] = outputProject;
    json[QStringLiteral("elapsed_ms")] = elapsedMs;

    QString summary = QStringLiteral("Sliced %1 sprites in %2 ms.")
        .arg(outFrames.size()).arg(elapsedMs);

    return CliResult::success(summary, json);
}

CliResult NativeCommands::executeFilter(const QStringList &args)
{
    QString inputPath;
    QString outputPath;
    QString filterName;

    // Despill parameters
    bool isDespill = false;
    QRgb fringeColor = 0;
    int despillTol = 20;
    QString despillModeStr = QStringLiteral("clamp");

    // Outline parameters
    bool isOutline = false;
    int outlineThick = 1;
    QRgb outlineColor = qRgb(0, 0, 0);
    bool silhouette = false;

    // Color swap parameters
    bool isColorSwap = false;
    QRgb srcColor = 0;
    QRgb dstColor = 0;
    int swapTol = 15;

    // Color adjust
    bool isColorAdjust = false;
    int hueShift = 0;
    int satMult = 100;
    int conMult = 100;

    // Rescale
    bool isPixelRescale = false;
    int rescaleFactor = 2;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--output") && i + 1 < args.size()) {
            outputPath = args[++i];
        } else if (arg == QStringLiteral("--despill")) {
            isDespill = true;
            filterName = QStringLiteral("despill");
            if (i + 1 < args.size() && !args[i + 1].startsWith(QStringLiteral("-"))) {
                QColor c(args[++i]);
                if (c.isValid()) fringeColor = c.rgb();
            }
        } else if (arg == QStringLiteral("--despill-mode") && i + 1 < args.size()) {
            QString m = args[++i].toLower();
            if (m == QStringLiteral("strict")) despillModeStr = QStringLiteral("strict");
        } else if (arg == QStringLiteral("--tolerance") && i + 1 < args.size()) {
            despillTol = args[++i].toInt();
            swapTol = despillTol;
        } else if (arg == QStringLiteral("--outline")) {
            isOutline = true;
            filterName = QStringLiteral("outline");
            if (i + 1 < args.size() && !args[i + 1].startsWith(QStringLiteral("-"))) {
                outlineThick = args[++i].toInt();
            }
        } else if (arg == QStringLiteral("--outline-color") && i + 1 < args.size()) {
            QColor c(args[++i]);
            if (c.isValid()) outlineColor = c.rgb();
        } else if (arg == QStringLiteral("--silhouette")) {
            silhouette = true;
        } else if (arg == QStringLiteral("--color-swap") && i + 1 < args.size()) {
            isColorSwap = true;
            filterName = QStringLiteral("color_swap");
            QString pair = args[++i];
            QStringList parts = pair.split(QLatin1Char(':'));
            if (parts.size() == 2) {
                srcColor = QColor(parts[0]).rgb();
                dstColor = QColor(parts[1]).rgb();
            }
        } else if (arg == QStringLiteral("--color-adjust")) {
            isColorAdjust = true;
            filterName = QStringLiteral("color_adjust");
        } else if (arg == QStringLiteral("--hue") && i + 1 < args.size()) {
            hueShift = args[++i].toInt();
        } else if (arg == QStringLiteral("--saturation") && i + 1 < args.size()) {
            satMult = args[++i].toInt();
        } else if (arg == QStringLiteral("--contrast") && i + 1 < args.size()) {
            conMult = args[++i].toInt();
        } else if (arg == QStringLiteral("--pixel-rescale") && i + 1 < args.size()) {
            isPixelRescale = true;
            filterName = QStringLiteral("pixel_rescale");
            QString factorStr = args[++i];
            factorStr.remove(QLatin1Char('x'));
            rescaleFactor = factorStr.toInt();
        } else if (arg == QStringLiteral("--filter") && i + 1 < args.size()) {
            filterName = args[++i];
        } else if (!arg.startsWith(QStringLiteral("-"))) {
            inputPath = arg;
        }
    }

    if (inputPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No input file specified for filter command."));
    }

    if (filterName.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No filter operation specified. Use --outline, --despill, --color-swap, --color-adjust, or --pixel-rescale."));
    }

    QFileInfo fi(inputPath);
    if (!fi.exists()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Input file not found: ") + inputPath);
    }

    if (outputPath.isEmpty()) {
        outputPath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral("_filtered.") + fi.suffix());
    }

    QImage img(inputPath);
    if (img.isNull()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Cannot load image: ") + inputPath);
    }
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    FilterPlugin *plugin = FilterRegistry::instance().findFilter(filterName);
    if (!plugin) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("Unknown or unavailable filter plugin: ") + filterName);
    }

    QVariantMap params;
    if (isDespill) {
        if (fringeColor == 0) fringeColor = ProjectController::detectDominantBackgroundColor(img);
        params[QStringLiteral("fringeColor")] = static_cast<uint>(fringeColor);
        params[QStringLiteral("tolerance")] = despillTol;
        params[QStringLiteral("mode")] = despillModeStr;
    } else if (isOutline) {
        params[QStringLiteral("thickness")] = outlineThick;
        params[QStringLiteral("color")] = static_cast<uint>(outlineColor);
        params[QStringLiteral("silhouette")] = silhouette;
    } else if (isColorSwap) {
        params[QStringLiteral("srcColor")] = static_cast<uint>(srcColor);
        params[QStringLiteral("dstColor")] = static_cast<uint>(dstColor);
        params[QStringLiteral("tolerance")] = swapTol;
        params[QStringLiteral("preserveShading")] = true;
    } else if (isColorAdjust) {
        params[QStringLiteral("hue")] = hueShift;
        params[QStringLiteral("saturation")] = satMult;
        params[QStringLiteral("contrast")] = conMult;
    } else if (isPixelRescale) {
        params[QStringLiteral("factor")] = rescaleFactor;
    }

    QElapsedTimer timer;
    timer.start();

    QImage resultImg = plugin->applyImage(img, params);

    qint64 elapsedMs = timer.elapsed();

    QFileInfo outFi(outputPath);
    QDir().mkpath(outFi.dir().absolutePath());
    if (!resultImg.save(outputPath, "PNG")) {
        return CliResult::error(ExitIoError, QStringLiteral("Failed to write output image: ") + outputPath);
    }

    QJsonObject json;
    json[QStringLiteral("input")] = inputPath;
    json[QStringLiteral("output")] = outputPath;
    json[QStringLiteral("filter")] = filterName;
    json[QStringLiteral("elapsed_ms")] = elapsedMs;

    QString summary = QStringLiteral("Filter '%1' applied to %2 in %3 ms.")
        .arg(filterName, outputPath).arg(elapsedMs);

    return CliResult::success(summary, json);
}

CliResult NativeCommands::executeSsp(const QStringList &args)
{
    QString sspPath;
    QString exportFormat;
    QString exportOutput;
    QString checkoutRevision;
    bool infoOnly = false;

    for (int i = 0; i < args.size(); ++i) {
        const QString &arg = args[i];
        if (arg == QStringLiteral("--info")) {
            infoOnly = true;
        } else if (arg == QStringLiteral("--export") && i + 1 < args.size()) {
            exportFormat = args[++i].toLower();
        } else if (arg == QStringLiteral("--output") && i + 1 < args.size()) {
            exportOutput = args[++i];
        } else if (arg == QStringLiteral("--checkout-revision") && i + 1 < args.size()) {
            checkoutRevision = args[++i];
        } else if (!arg.startsWith(QStringLiteral("-"))) {
            sspPath = arg;
        }
    }
    Q_UNUSED(infoOnly);

    if (sspPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No .ssp project file specified."));
    }

    QFileInfo fi(sspPath);
    if (!fi.exists()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral(".ssp file not found: ") + sspPath);
    }

    SpriteDocument doc;
    ProjectController controller(&doc);
    QString err;
    if (!controller.openProject(sspPath, &err)) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Failed to open .ssp: ") + err);
    }

    if (!checkoutRevision.isEmpty()) {
        if (!controller.checkoutRevision(checkoutRevision, &err)) {
            return CliResult::error(ExitConstraintFailed, QStringLiteral("Checkout failed: ") + err);
        }
    }

    if (!exportFormat.isEmpty() && !exportOutput.isEmpty()) {
        ExportOptions opts;
        if (exportFormat == QStringLiteral("godot") || exportFormat == QStringLiteral("godot4")) {
            opts.format = FORMAT_GODOT;
        } else {
            opts.format = FORMAT_TEXTUREPACKER_JSON;
        }

        ProjectController controller(&doc);
        if (!controller.exportData(exportOutput, opts, &err)) {
            return CliResult::error(ExitIoError, QStringLiteral("Export failed: ") + err);
        }

        QJsonObject json;
        json[QStringLiteral("project")] = sspPath;
        json[QStringLiteral("exported_to")] = exportOutput;
        json[QStringLiteral("format")] = exportFormat;
        return CliResult::success(QStringLiteral("Exported project to ") + exportOutput, json);
    }

    QJsonObject json;
    json[QStringLiteral("project")] = sspPath;
    json[QStringLiteral("frames_count")] = doc.frameCount();
    json[QStringLiteral("animations_count")] = doc.animations().size();
    json[QStringLiteral("atlas_width")] = doc.atlas().width();
    json[QStringLiteral("atlas_height")] = doc.atlas().height();

    QJsonArray animsArray;
    for (auto it = doc.animations().begin(); it != doc.animations().end(); ++it) {
        QJsonObject aObj;
        aObj[QStringLiteral("name")] = it.key();
        aObj[QStringLiteral("frames")] = it.value().frameIndices.size();
        aObj[QStringLiteral("fps")] = it.value().fps;
        aObj[QStringLiteral("loop")] = it.value().loop;
        animsArray.append(aObj);
    }
    json[QStringLiteral("animations")] = animsArray;

    QString summary = QStringLiteral("Project '%1': %2 frames, %3 animations.")
        .arg(fi.fileName()).arg(doc.frameCount()).arg(doc.animations().size());

    return CliResult::success(summary, json);
}

} // namespace SpriteStudioCli
