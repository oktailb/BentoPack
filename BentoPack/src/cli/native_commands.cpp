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

#include "cli/native_commands.h"
#include "cli/clipackpipeline.h"
#include "image/spritedetector.h"
#include "project/sessionmanager.h"
#include "controller/projectcontroller.h"
#include "filters/filterplugin.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include "generated/version.h"
#include "simd/cpu_features.h"
#include "simd/simd_kernels.h"
#include "image/pixelhash.h"
#include "model/spritedocument.h"
#include <QFileInfo>
#include <QDir>
#include <QColor>
#include <QPainter>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <cmath>

namespace BentoPackCli {

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

    // Save project .bento
    if (!outputProject.isEmpty()) {
        SpriteDocument doc;
        doc.setFilePath(outputProject);
        doc.setAtlas(sourceImg);
        doc.setFrames(outFrames, outBoxes);
        ProjectController controller(&doc);
        QString err;
        if (!controller.saveProject(outputProject, &err)) {
            return CliResult::error(ExitIoError, QStringLiteral("Failed to save .bento project: ") + err);
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

    double mpxSec = (elapsedMs > 0) ? ((double(img.width() * img.height()) / 1000000.0) / (double(elapsedMs) / 1000.0)) : 0.0;

    QJsonObject json;
    json[QStringLiteral("input")] = inputPath;
    json[QStringLiteral("output")] = outputPath;
    json[QStringLiteral("filter")] = filterName;
    json[QStringLiteral("width")] = img.width();
    json[QStringLiteral("height")] = img.height();
    json[QStringLiteral("pixels")] = img.width() * img.height();
    json[QStringLiteral("simd_arch")] = QString::fromUtf8(CpuFeatures::simdArchitectureName());
    json[QStringLiteral("elapsed_ms")] = elapsedMs;
    json[QStringLiteral("throughput_mpx_sec")] = std::round(mpxSec * 10.0) / 10.0;

    QString summary = QStringLiteral("Filter '%1' applied to %2 (%3x%4, %5 ms, %6 Mpx/s, SIMD: %7).")
        .arg(filterName, outputPath)
        .arg(img.width()).arg(img.height())
        .arg(elapsedMs)
        .arg(QString::number(mpxSec, 'f', 1))
        .arg(QString::fromUtf8(CpuFeatures::simdArchitectureName()));

    return CliResult::success(summary, json);
}

CliResult NativeCommands::executeBento(const QStringList &args)
{
    QString bentoPath;
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
            bentoPath = arg;
        }
    }
    Q_UNUSED(infoOnly);

    if (bentoPath.isEmpty()) {
        return CliResult::error(ExitSyntaxError, QStringLiteral("No .bento (or .bento) project file specified."));
    }

    QFileInfo fi(bentoPath);
    if (!fi.exists()) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Project file not found: ") + bentoPath);
    }

    SpriteDocument doc;
    ProjectController controller(&doc);
    QString err;
    if (!controller.openProject(bentoPath, &err)) {
        return CliResult::error(ExitFileNotFound, QStringLiteral("Failed to open project: ") + err);
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
        } else if (exportFormat == QStringLiteral("unity")) {
            opts.format = FORMAT_UNITY;
        } else if (exportFormat == QStringLiteral("unreal") || exportFormat == QStringLiteral("paper2d")) {
            opts.format = FORMAT_UNREAL;
        } else {
            opts.format = FORMAT_TEXTUREPACKER_JSON;
        }
        opts.packOptions.algorithm = AtlasPacker::KeepLayout;

        ProjectController controller(&doc);
        if (!controller.exportData(exportOutput, opts, &err)) {
            return CliResult::error(ExitIoError, QStringLiteral("Export failed: ") + err);
        }

        QJsonObject json;
        json[QStringLiteral("project")] = bentoPath;
        json[QStringLiteral("exported_to")] = exportOutput;
        json[QStringLiteral("format")] = exportFormat;
        return CliResult::success(QStringLiteral("Exported project to ") + exportOutput, json);
    }

    QJsonObject json;
    json[QStringLiteral("project")] = bentoPath;
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

CliResult NativeCommands::executeSchema()
{
    QJsonObject schema;
    schema[QStringLiteral("cli")] = QStringLiteral("bentopack-cli");
    schema[QStringLiteral("version")] = QStringLiteral(PROJECT_VERSION);
    schema[QStringLiteral("description")] = QStringLiteral("Headless 2D sprite atlas packer, slicer, filter and game engine asset compiler.");

    // Exit codes
    QJsonObject exitCodes;
    exitCodes[QStringLiteral("0")] = QStringLiteral("ExitSuccess: Execution completed without errors.");
    exitCodes[QStringLiteral("1")] = QStringLiteral("ExitSyntaxError: Invalid arguments, unknown options, or missing parameters.");
    exitCodes[QStringLiteral("2")] = QStringLiteral("ExitFileNotFound: Input file, directory or asset does not exist.");
    exitCodes[QStringLiteral("3")] = QStringLiteral("ExitConstraintFailed: Packing constraints unsatisfied (e.g. sprites exceed max-size).");
    exitCodes[QStringLiteral("4")] = QStringLiteral("ExitIoError: Disk write error, permission failure, or corrupt output path.");
    exitCodes[QStringLiteral("5")] = QStringLiteral("ExitLockConflict: Another watch daemon instance is already active for this directory.");
    exitCodes[QStringLiteral("6")] = QStringLiteral("ExitPluginNotFound: Requested export plugin or format encoder not found.");
    schema[QStringLiteral("exit_codes")] = exitCodes;

    // Supported capabilities
    QJsonArray algorithms;
    algorithms.append(QStringLiteral("MaxRects"));
    algorithms.append(QStringLiteral("Basic"));
    algorithms.append(QStringLiteral("Grid"));
    algorithms.append(QStringLiteral("TightPolygon"));
    schema[QStringLiteral("algorithms")] = algorithms;

    QJsonArray heuristics;
    heuristics.append(QStringLiteral("BestShortSideFit"));
    heuristics.append(QStringLiteral("BestLongSideFit"));
    heuristics.append(QStringLiteral("BestAreaFit"));
    heuristics.append(QStringLiteral("BottomLeft"));
    heuristics.append(QStringLiteral("ContactPoint"));
    schema[QStringLiteral("heuristics")] = heuristics;

    QJsonArray formats;
    formats.append(QStringLiteral("json-array"));
    formats.append(QStringLiteral("json-hash"));
    formats.append(QStringLiteral("godot4"));
    formats.append(QStringLiteral("unity"));
    formats.append(QStringLiteral("unreal"));
    formats.append(QStringLiteral("libgdx"));
    formats.append(QStringLiteral("spine"));
    formats.append(QStringLiteral("bento"));
    schema[QStringLiteral("export_formats")] = formats;

    QJsonArray texFormats;
    texFormats.append(QStringLiteral("png"));
    texFormats.append(QStringLiteral("webp"));
    texFormats.append(QStringLiteral("ktx2"));
    texFormats.append(QStringLiteral("basis"));
    schema[QStringLiteral("texture_formats")] = texFormats;

    // Subcommands definition
    QJsonObject cmds;

    // pack
    QJsonObject packCmd;
    packCmd[QStringLiteral("description")] = QStringLiteral("Pack loose images, folders, or sprite files into an optimized atlas sheet and metadata.");
    QJsonObject packOpts;
    packOpts[QStringLiteral("--sheet")] = QStringLiteral("Target atlas image file (e.g. atlas.png, atlas.ktx2)");
    packOpts[QStringLiteral("--data")] = QStringLiteral("Target descriptor metadata file (e.g. atlas.json, atlas.tres)");
    packOpts[QStringLiteral("--format")] = QStringLiteral("Data export format (json-array, json-hash, godot4, unity, unreal, libgdx)");
    packOpts[QStringLiteral("--algorithm")] = QStringLiteral("Packing algorithm: MaxRects (default), Basic, Grid, TightPolygon");
    packOpts[QStringLiteral("--maxrects-heuristics")] = QStringLiteral("Heuristic: BestShortSideFit (default), BestLongSideFit, BestAreaFit, BottomLeft, ContactPoint");
    packOpts[QStringLiteral("--size-constraints")] = QStringLiteral("Size constraint: AnySize (default) or POT (Power of Two)");
    packOpts[QStringLiteral("--max-size")] = QStringLiteral("Max width and height: <w> <h> (e.g. 2048 2048)");
    packOpts[QStringLiteral("--padding")] = QStringLiteral("Pixel spacing between sprite rects (default: 2)");
    packOpts[QStringLiteral("--border-padding")] = QStringLiteral("Pixel margin around atlas borders (default: 0)");
    packOpts[QStringLiteral("--extrude")] = QStringLiteral("Duplicate border pixels to prevent texture bleeding on scaling (default: 0)");
    packOpts[QStringLiteral("--trim-mode")] = QStringLiteral("Trim transparent pixels: None (default), Trim, Crop");
    packOpts[QStringLiteral("--trim-threshold")] = QStringLiteral("Alpha value below which pixels are considered transparent (0-255, default: 1)");
    packOpts[QStringLiteral("--enable-auto-alias")] = QStringLiteral("Deduplicate identical sprite frames by pixel hashing");
    packOpts[QStringLiteral("--pivot-point")] = QStringLiteral("Normalized pivot anchor: <x> <y> (e.g. 0.5 1.0 for feet)");
    packOpts[QStringLiteral("--godot-uid")] = QStringLiteral("Explicit Godot 4 UID to embed or preserve in .tres");
    packOpts[QStringLiteral("--godot-scene")] = QStringLiteral("Generate ready-to-instantiate Godot 4 AnimatedSprite2D scene (.tscn)");
    packOpts[QStringLiteral("--texture-format")] = QStringLiteral("Output texture encoder: png, webp, ktx2, basis");
    packOpts[QStringLiteral("--vram-format")] = QStringLiteral("VRAM compression codec: uastc (high-quality) or etc1s (compact)");
    packOpts[QStringLiteral("--vram-quality")] = QStringLiteral("Quality level for VRAM encoder (0-255, default: 128)");
    packOpts[QStringLiteral("--enable-zstd")] = QStringLiteral("Enable Zstandard supercompression on KTX2/Basis");
    packOpts[QStringLiteral("--zstd-level")] = QStringLiteral("Zstandard compression level (1-22, default: 9)");
    packOpts[QStringLiteral("--multipage")] = QStringLiteral("Enable multi-page atlas spanning for collections exceeding max dimensions (M16)");
    packCmd[QStringLiteral("options")] = packOpts;
    cmds[QStringLiteral("pack")] = packCmd;

    // slice
    QJsonObject sliceCmd;
    sliceCmd[QStringLiteral("description")] = QStringLiteral("Extract individual sprites from a composite sheet using smart connected-component detection.");
    QJsonObject sliceOpts;
    sliceOpts[QStringLiteral("<input_image>")] = QStringLiteral("Path to source composite sprite sheet image");
    sliceOpts[QStringLiteral("--output-dir")] = QStringLiteral("Directory where extracted PNG frames will be written");
    sliceOpts[QStringLiteral("--output-project")] = QStringLiteral("Save directly as a native BentoPack project (.bento)");
    sliceOpts[QStringLiteral("--remove-bg")] = QStringLiteral("Automatically detect and remove uniform background color");
    sliceOpts[QStringLiteral("--tolerance")] = QStringLiteral("Color distance tolerance for background removal (0-100, default: 10)");
    sliceOpts[QStringLiteral("--alpha-threshold")] = QStringLiteral("Alpha threshold for component detection (default: 1)");
    sliceOpts[QStringLiteral("--smart-crop")] = QStringLiteral("Crop bounding boxes tightly around non-transparent pixels");
    sliceOpts[QStringLiteral("--min-size")] = QStringLiteral("Minimum width and height to filter out noise pixels (default: 3)");
    sliceCmd[QStringLiteral("options")] = sliceOpts;
    cmds[QStringLiteral("slice")] = sliceCmd;

    // filter
    QJsonObject filterCmd;
    filterCmd[QStringLiteral("description")] = QStringLiteral("Apply non-destructive image processing filters in headless batch mode.");
    QJsonObject filterOpts;
    filterOpts[QStringLiteral("<input_image>")] = QStringLiteral("Path to source sprite or texture image");
    filterOpts[QStringLiteral("--output")] = QStringLiteral("Path for the processed output image");
    filterOpts[QStringLiteral("--despill")] = QStringLiteral("Remove colored halo/bleed fringes [optional: <hex_color>]");
    filterOpts[QStringLiteral("--despill-mode")] = QStringLiteral("Despill mode: clamp (default) or strict");
    filterOpts[QStringLiteral("--outline")] = QStringLiteral("Generate outer contour stroke [optional: <thickness_px>]");
    filterOpts[QStringLiteral("--outline-color")] = QStringLiteral("Color for outline stroke in CSS/Hex format (#RRGGBB)");
    filterOpts[QStringLiteral("--silhouette")] = QStringLiteral("Fill sprite interior with solid outline color");
    filterOpts[QStringLiteral("--color-swap")] = QStringLiteral("Replace source color with destination: <#src:#dst>");
    filterOpts[QStringLiteral("--color-adjust")] = QStringLiteral("Adjust HSL color balance and contrast");
    filterOpts[QStringLiteral("--hue")] = QStringLiteral("Hue rotation in degrees (-180 to +180)");
    filterOpts[QStringLiteral("--saturation")] = QStringLiteral("Saturation multiplier percentage (e.g. 150 for +50%)");
    filterOpts[QStringLiteral("--contrast")] = QStringLiteral("Contrast multiplier percentage (e.g. 120 for +20%)");
    filterOpts[QStringLiteral("--pixel-rescale")] = QStringLiteral("Pixel-art upscale using Nearest-Neighbor: 2x, 3x, 4x");
    filterCmd[QStringLiteral("options")] = filterOpts;
    cmds[QStringLiteral("filter")] = filterCmd;

    // bento
    QJsonObject bentoCmd;
    bentoCmd[QStringLiteral("description")] = QStringLiteral("Inspect, export, or manage native BentoPack project archives (.bento).");
    QJsonObject bentoOpts;
    bentoOpts[QStringLiteral("<project_file.bento>")] = QStringLiteral("Path to .bento project archive");
    bentoOpts[QStringLiteral("--info")] = QStringLiteral("Dump frame count, dimensions, animations, and metadata");
    bentoOpts[QStringLiteral("--export")] = QStringLiteral("Export project directly: godot4, unity, unreal, json");
    bentoOpts[QStringLiteral("--output")] = QStringLiteral("Target destination path for exported data");
    bentoOpts[QStringLiteral("--checkout-revision")] = QStringLiteral("Restore project state from embedded LibGit2 git commit SHA");
    bentoCmd[QStringLiteral("options")] = bentoOpts;
    cmds[QStringLiteral("bento")] = bentoCmd;

    // watch
    QJsonObject watchCmd;
    watchCmd[QStringLiteral("description")] = QStringLiteral("Watch folder in background and automatically re-pack on file change.");
    QJsonObject watchOpts;
    watchOpts[QStringLiteral("--watch")] = QStringLiteral("Start filesystem watcher on target inputs");
    watchOpts[QStringLiteral("--daemon")] = QStringLiteral("Run watcher as an isolated background daemon with .lock guarding");
    watchOpts[QStringLiteral("--debounce")] = QStringLiteral("Debounce delay in milliseconds before re-packing (default: 300)");
    watchOpts[QStringLiteral("--stop-watch")] = QStringLiteral("Gracefully terminate active watch daemon guarding target directory");
    watchCmd[QStringLiteral("options")] = watchOpts;
    cmds[QStringLiteral("watch")] = watchCmd;

    // benchmark
    QJsonObject benchCmd;
    benchCmd[QStringLiteral("description")] = QStringLiteral("Measure SIMD vectorization throughput & dirty-rect undo memory reduction.");
    QJsonObject benchOpts;
    benchOpts[QStringLiteral("--simd")] = QStringLiteral("Benchmark SIMD kernels vs Scalar baseline (speedup & Mpx/s)");
    benchOpts[QStringLiteral("--dirty-rect")] = QStringLiteral("Benchmark 4K atlas dirty-rect compute & memory savings");
    benchOpts[QStringLiteral("--all")] = QStringLiteral("Run all hardware & memory performance benchmarks (default)");
    benchCmd[QStringLiteral("options")] = benchOpts;
    cmds[QStringLiteral("benchmark")] = benchCmd;

    schema[QStringLiteral("commands")] = cmds;

    return CliResult::success(QStringLiteral("BentoPack CLI schema"), schema);
}

CliResult NativeCommands::executeBenchmark(const QStringList &args)
{
    bool runSimd = args.contains(QStringLiteral("--simd")) || args.isEmpty() || args.contains(QStringLiteral("--all"));
    bool runDirtyRect = args.contains(QStringLiteral("--dirty-rect")) || args.isEmpty() || args.contains(QStringLiteral("--all"));
    if (args.contains(QStringLiteral("--simd")) && !args.contains(QStringLiteral("--dirty-rect"))) {
        runDirtyRect = false;
    }
    if (args.contains(QStringLiteral("--dirty-rect")) && !args.contains(QStringLiteral("--simd"))) {
        runSimd = false;
    }

    QJsonObject result;
    result[QStringLiteral("status")] = QStringLiteral("success");
    result[QStringLiteral("simd_arch")] = QString::fromUtf8(CpuFeatures::simdArchitectureName());

    if (runSimd) {
        QJsonObject simdObj;
        simdObj[QStringLiteral("arch")] = QString::fromUtf8(CpuFeatures::simdArchitectureName());
        simdObj[QStringLiteral("has_avx2")] = CpuFeatures::hasAvx2();
        simdObj[QStringLiteral("has_neon")] = CpuFeatures::hasNeon();

        const int pixelCount = 1048576; // 1 Megapixel
        const int iterations = 10;      // 10 Megapixels total tested
        QVector<QRgb> srcBuffer(pixelCount);
        QVector<QRgb> dstScalar(pixelCount);
        QVector<QRgb> dstSimd(pixelCount);

        for (int i = 0; i < pixelCount; ++i) {
            int m = i % 7;
            if (m == 0) srcBuffer[i] = qRgba(0, 200, 0, 255);
            else if (m == 1) srcBuffer[i] = qRgba(10, 195, 5, 255);
            else if (m == 2) srcBuffer[i] = qRgba(10, 195, 5, 0);
            else if (m == 3) srcBuffer[i] = qRgba(200, 20, 20, 255);
            else if (m == 4) srcBuffer[i] = qRgba(20, 20, 200, 255);
            else if (m == 5) srcBuffer[i] = qRgba(30, 210, 40, 255);
            else srcBuffer[i] = qRgba(240, 240, 240, 255);
        }

        // 1. ColorSwap Benchmark
        const QRgb srcTarget = qRgb(0, 200, 0);
        const QRgb dstTarget = qRgb(220, 20, 20);
        const int tolerance = 30;
        const double maxDist = tolerance * 4.4167;
        const int maxDistSq = static_cast<int>(std::round(maxDist * maxDist));
        const QRgb dstRgbBase = qRgb(qRed(dstTarget), qGreen(dstTarget), qBlue(dstTarget));
        QColor srcCol(srcTarget);
        QColor dstCol(dstTarget);
        const int sV = srcCol.value();
        const int sS = srcCol.hsvSaturation();
        const int dH = dstCol.hsvHue();
        const int dS = dstCol.hsvSaturation();
        const int dV = dstCol.value();

        dstScalar = srcBuffer;
        QElapsedTimer timerScalar;
        timerScalar.start();
        for (int it = 0; it < iterations; ++it) {
            SimdKernels::colorSwapRow_scalar(srcBuffer.constData(), dstScalar.data(), pixelCount,
                                             qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                             maxDistSq, dstRgbBase, false, sV, sS, dH, dS, dV);
        }
        qint64 scalarElapsedNs = timerScalar.nsecsElapsed();
        double scalarMs = scalarElapsedNs / 1000000.0;

        dstSimd = srcBuffer;
        QElapsedTimer timerSimd;
        timerSimd.start();
        for (int it = 0; it < iterations; ++it) {
            SimdKernels::colorSwapRow(srcBuffer.constData(), dstSimd.data(), pixelCount,
                                      qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                      maxDistSq, dstRgbBase, false, sV, sS, dH, dS, dV);
        }
        qint64 simdElapsedNs = timerSimd.nsecsElapsed();
        double simdMs = simdElapsedNs / 1000000.0;

        bool bitExact = true;
        for (int i = 0; i < pixelCount; ++i) {
            if (dstSimd[i] != dstScalar[i]) {
                bitExact = false;
                break;
            }
        }

        double speedup = (simdMs > 0.0) ? (scalarMs / simdMs) : 1.0;
        double mpxSec = (simdMs > 0.0) ? ((double(pixelCount * iterations) / 1000000.0) / (simdMs / 1000.0)) : 0.0;

        QJsonObject csJson;
        csJson[QStringLiteral("pixels_tested")] = pixelCount * iterations;
        csJson[QStringLiteral("simd_ms")] = std::round(simdMs * 100.0) / 100.0;
        csJson[QStringLiteral("scalar_ms")] = std::round(scalarMs * 100.0) / 100.0;
        csJson[QStringLiteral("speedup")] = std::round(speedup * 100.0) / 100.0;
        csJson[QStringLiteral("throughput_mpx_sec")] = std::round(mpxSec * 10.0) / 10.0;
        csJson[QStringLiteral("bit_exact")] = bitExact;
        simdObj[QStringLiteral("color_swap")] = csJson;

        // 2. Despill Chroma Benchmark
        dstScalar = srcBuffer;
        timerScalar.restart();
        for (int it = 0; it < iterations; ++it) {
            SimdKernels::despillChromaRow_scalar(srcBuffer.constData(), dstScalar.data(), pixelCount,
                                                true, false, 1.0);
        }
        scalarElapsedNs = timerScalar.nsecsElapsed();
        scalarMs = scalarElapsedNs / 1000000.0;

        dstSimd = srcBuffer;
        timerSimd.restart();
        for (int it = 0; it < iterations; ++it) {
            SimdKernels::despillChromaRow(srcBuffer.constData(), dstSimd.data(), pixelCount,
                                         true, false, 1.0);
        }
        simdElapsedNs = timerSimd.nsecsElapsed();
        simdMs = simdElapsedNs / 1000000.0;

        bitExact = true;
        for (int i = 0; i < pixelCount; ++i) {
            if (dstSimd[i] != dstScalar[i]) {
                bitExact = false;
                break;
            }
        }

        speedup = (simdMs > 0.0) ? (scalarMs / simdMs) : 1.0;
        mpxSec = (simdMs > 0.0) ? ((double(pixelCount * iterations) / 1000000.0) / (simdMs / 1000.0)) : 0.0;

        QJsonObject dsJson;
        dsJson[QStringLiteral("pixels_tested")] = pixelCount * iterations;
        dsJson[QStringLiteral("simd_ms")] = std::round(simdMs * 100.0) / 100.0;
        dsJson[QStringLiteral("scalar_ms")] = std::round(scalarMs * 100.0) / 100.0;
        dsJson[QStringLiteral("speedup")] = std::round(speedup * 100.0) / 100.0;
        dsJson[QStringLiteral("throughput_mpx_sec")] = std::round(mpxSec * 10.0) / 10.0;
        dsJson[QStringLiteral("bit_exact")] = bitExact;
        simdObj[QStringLiteral("despill")] = dsJson;

        result[QStringLiteral("simd")] = simdObj;
    }

    if (runDirtyRect) {
        QJsonObject dirtyObj;
        const int atlasDim = 4096;
        const qint64 fullBytes = qint64(atlasDim) * qint64(atlasDim) * 4; // 67,108,864 bytes
        const int patchDim = 128;
        const qint64 patchBytes = qint64(patchDim) * qint64(patchDim) * 4; // 65,536 bytes
        const double memorySaved = 100.0 * (1.0 - (double(patchBytes) / double(fullBytes)));

        QImage atlasA(atlasDim, atlasDim, QImage::Format_ARGB32);
        atlasA.fill(qRgb(15, 15, 20));
        QImage atlasB = atlasA.copy();

        QRect editRect(1200, 1400, patchDim, patchDim);
        QImage patchFrame(patchDim, patchDim, QImage::Format_ARGB32);
        patchFrame.fill(qRgb(220, 40, 40));
        {
            QPainter p(&atlasB);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.drawImage(editRect.topLeft(), patchFrame);
        }

        QElapsedTimer timerDirty;
        timerDirty.start();
        QRect detectedRect = PixelHash::computeDirtyRect(atlasA, atlasB);
        qint64 computeNs = timerDirty.nsecsElapsed();
        double computeUs = computeNs / 1000.0;

        SpriteDocument doc;
        doc.setAtlas(atlasA);
        QElapsedTimer timerPatch;
        timerPatch.start();
        doc.patchAtlasDirect(editRect, patchFrame);
        qint64 patchNs = timerPatch.nsecsElapsed();
        double patchUs = patchNs / 1000.0;

        dirtyObj[QStringLiteral("atlas_dimensions")] = QStringLiteral("4096x4096");
        dirtyObj[QStringLiteral("full_atlas_bytes")] = fullBytes;
        dirtyObj[QStringLiteral("full_atlas_mb")] = std::round((double(fullBytes) / (1024.0 * 1024.0)) * 10.0) / 10.0;
        dirtyObj[QStringLiteral("dirty_rect_dimensions")] = QStringLiteral("128x128");
        dirtyObj[QStringLiteral("dirty_rect_bytes")] = patchBytes;
        dirtyObj[QStringLiteral("dirty_rect_kb")] = std::round((double(patchBytes) / 1024.0) * 10.0) / 10.0;
        dirtyObj[QStringLiteral("memory_saved_percent")] = std::round(memorySaved * 100.0) / 100.0;
        dirtyObj[QStringLiteral("compute_dirty_rect_us")] = std::round(computeUs * 10.0) / 10.0;
        dirtyObj[QStringLiteral("patch_atlas_us")] = std::round(patchUs * 10.0) / 10.0;
        dirtyObj[QStringLiteral("exact_match")] = (detectedRect == editRect);

        result[QStringLiteral("dirty_rect")] = dirtyObj;
    }

    QString summary = QStringLiteral("Benchmark executed (SIMD: %1).")
        .arg(QString::fromUtf8(CpuFeatures::simdArchitectureName()));

    return CliResult::success(summary, result);
}

} // namespace BentoPackCli
