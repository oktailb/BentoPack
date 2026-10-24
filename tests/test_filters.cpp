#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QMenu>
#include <QAction>
#include <QTabWidget>
#include <QTextEdit>
#include <QPalette>
#include <QApplication>
#include <cstdio>

#include "model/spritedocument.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include "backgroundremovaldialog.h"
#include "despillfilterdialog.h"
#include "outlinefilterdialog.h"
#include "colorswapfilterdialog.h"
#include "coloradjustfilterdialog.h"
#include "pixelrescalefilterdialog.h"
#include "retropalettefilterdialog.h"
#include "atlaspackingdialog.h"
#include "atlaspackingfilter.h"
#include "spritedetectiondialog.h"
#include "spritedetectionfilter.h"
#include "normalgeneratordialog.h"
#include "normalgeneratorfilter.h"
#include "emissivegeneratordialog.h"
#include "emissivegeneratorfilter.h"
#include "packer/atlaspacker.h"
#include "commands/filtercommands.h"
#include "widgets/filtermenubuilder.h"
#include "aboutdialog.h"
#include "localizationmanager.h"
#include "config/appconfig.h"
#include "image/pixelhash.h"
#include "simd/cpu_features.h"
#include "simd/simd_kernels.h"

class TestFilters : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFilterRegistry();
    void testDespillFilterAlgorithm();
    void testDespillFilterChromaSpill();
    void testNormalGeneratorAlgorithm();
    void testEmissiveGeneratorAlgorithm();
    void testM21AuxiliaryMapUndoRedo();
    void testAddMaterialLayerCommandUndoRedo();
    void testAtlasPackerCornersClampingAndExtrusion();
    void testOutlineFilterAlgorithm();
    void testColorSwapFilterAlgorithm();
    void testColorAdjustFilterAlgorithm();
    void testPixelRescaleFilterAlgorithm();
    void testRetroPaletteFilterAlgorithm();
    void testApplyFilterCommandUndoRedo();
    void testComputeDirtyRect();
    void testDifferentialAtlasFilterUndoRedo();
    void testCpuFeatures();
    void testSimdColorSwapRowAccuracy();
    void testSimdDespillChromaRowAccuracy();
    void testFilterAutoDetectBoxes();
    void testAtlasPackingFilterInteractive();
    void testSpriteDetectionFilterInteractive();
    void testAboutDialogDarkModeAndPricing();
};

void TestFilters::initTestCase()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString binPlugins = QDir(appDir).filePath(QStringLiteral("plugins"));
    ExtractorRegistry::instance().loadPlugins(binPlugins);
    FilterRegistry::instance().loadPlugins(binPlugins);
    ExtractorRegistry::instance().loadPlugins(appDir);
    FilterRegistry::instance().loadPlugins(appDir);
}

void TestFilters::cleanupTestCase()
{
}

void TestFilters::testFilterRegistry()
{
    FilterRegistry &reg = FilterRegistry::instance();
    reg.initDefaultFilters();
    reg.loadPlugins(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins")));

    // Multiple calls to init/load should never create duplicates
    reg.initDefaultFilters();
    QCOMPARE(reg.filters().size(), 12);
    QVERIFY(reg.findFilter(QStringLiteral("background_removal")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("despill")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("outline")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("color_swap")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("color_adjust")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("pixel_rescale")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("retro_palette")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("atlas_packing")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("tight_polygon_packing")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("sprite_detection")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("normalgenerator")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("emissivegenerator")) != nullptr);

    QStringList cats = reg.categories();
    QVERIFY(!cats.isEmpty());

    QMenu testMenu;
    SpriteDocument doc;
    QUndoStack undoStack;
    FilterMenuBuilder::populateMenu(&testMenu, &doc, &undoStack, nullptr);
    
    // Count non-separator, non-section menu actions (the actual filter items)
    int filterActionCount = 0;
    for (QAction *act : testMenu.actions()) {
        if (!act->isSeparator() && act->menuRole() != QAction::ApplicationSpecificRole) {
            filterActionCount++;
        }
    }
    // Exactly 12 filter actions without any duplicates
    QCOMPARE(filterActionCount, 12);

    // Test taxonomy and modifier flags
    FilterPlugin *bgRem = reg.findFilter(QStringLiteral("background_removal"));
    QVERIFY(bgRem != nullptr);
    QVERIFY(bgRem->isGeometryModifier());
    QVERIFY(bgRem->isPixelModifier());
    QVERIFY(bgRem->isAtlasModifier());

    FilterPlugin *outline = reg.findFilter(QStringLiteral("outline"));
    QVERIFY(outline != nullptr);
    QVERIFY(outline->isGeometryModifier());

    FilterPlugin *despill = reg.findFilter(QStringLiteral("despill"));
    QVERIFY(despill != nullptr);
    QVERIFY(despill->isPixelModifier());
    QVERIFY(!despill->isGeometryModifier());
    QVERIFY(!despill->isAtlasModifier());

    FilterPlugin *atlasPack = reg.findFilter(QStringLiteral("atlas_packing"));
    QVERIFY(atlasPack != nullptr);
    QVERIFY(atlasPack->isAtlasModifier());
    QVERIFY(!atlasPack->isGeometryModifier());

    FilterPlugin *tightPack = reg.findFilter(QStringLiteral("tight_polygon_packing"));
    QVERIFY(tightPack != nullptr);
    QVERIFY(tightPack->isAtlasModifier());
    QVERIFY(tightPack->isGeometryModifier());

    FilterPlugin *spriteDetect = reg.findFilter(QStringLiteral("sprite_detection"));
    QVERIFY(spriteDetect != nullptr);
    QVERIFY(spriteDetect->isGeometryModifier());
    QVERIFY(!spriteDetect->isPixelModifier());
    QVERIFY(!spriteDetect->isAtlasModifier());

    QList<FilterPlugin*> pixelMods = reg.filtersByModifier(FilterPlugin::PixelModifier);
    QVERIFY(!pixelMods.isEmpty());
    QList<FilterPlugin*> geomMods = reg.filtersByModifier(FilterPlugin::GeometryModifier);
    QVERIFY(!geomMods.isEmpty());
    QList<FilterPlugin*> atlasMods = reg.filtersByModifier(FilterPlugin::AtlasModifier);
    QCOMPARE(atlasMods.size(), 3);
}

void TestFilters::testDespillFilterAlgorithm()
{
    QImage src(20, 20, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb greenFringe = qRgb(0, 255, 0);
    QRgb redBody = qRgb(255, 0, 0);

    for (int y = 6; y <= 13; ++y) {
        for (int x = 6; x <= 13; ++x) {
            src.setPixel(x, y, redBody);
        }
    }
    for (int x = 5; x <= 14; ++x) {
        src.setPixel(x, 5, greenFringe);
        src.setPixel(x, 14, greenFringe);
    }
    for (int y = 5; y <= 14; ++y) {
        src.setPixel(5, y, greenFringe);
        src.setPixel(14, y, greenFringe);
    }

    // 1. StrictAlpha mode
    int modifiedAlpha = 0;
    QImage resAlpha = DespillFilterDialog::applyDespill(src, greenFringe, 30, DespillFilterDialog::StrictAlpha, {}, &modifiedAlpha);
    QVERIFY(modifiedAlpha > 0);
    QCOMPARE(qAlpha(resAlpha.pixel(5, 5)), 0);
    QCOMPARE(qAlpha(resAlpha.pixel(6, 6)), 255);
    QCOMPARE(resAlpha.pixel(6, 6), redBody);

    // 2. ColorClamping mode: green fringe pixel is converted to adjacent red
    int modifiedClamp = 0;
    QImage resClamp = DespillFilterDialog::applyDespill(src, greenFringe, 30, DespillFilterDialog::ColorClamping, {}, &modifiedClamp);
    QVERIFY(modifiedClamp > 0);
    QCOMPARE(qAlpha(resClamp.pixel(5, 6)), 255);
    QCOMPARE(qRed(resClamp.pixel(5, 6)), 255);
    QCOMPARE(qGreen(resClamp.pixel(5, 6)), 0);
}

void TestFilters::testDespillFilterChromaSpill()
{
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    // Warm flesh / red cloak pixel with green bounce spill from green screen
    QRgb greenBouncePixel = qRgb(170, 210, 80); // Green is artificially high due to spill
    src.setPixel(5, 5, greenBouncePixel);

    int modifiedSpill = 0;
    QImage res = DespillFilterDialog::applyDespill(
        src, qRgb(0, 255, 0), 40, DespillFilterDialog::ChromaSpill, {}, &modifiedSpill);

    QVERIFY(modifiedSpill > 0);
    QRgb outPx = res.pixel(5, 5);
    QCOMPARE(qAlpha(outPx), 255); // Alpha is strictly preserved in ChromaSpill
    // Chroma spill formula: G' = min(G, max(R, B)) = min(210, 170) = 170
    QCOMPARE(qRed(outPx), 170);
    QCOMPARE(qGreen(outPx), 170);
    QCOMPARE(qBlue(outPx), 80);
}

void TestFilters::testNormalGeneratorAlgorithm()
{
    QImage src(16, 16, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    for (int y = 4; y <= 11; ++y) {
        for (int x = 4; x <= 11; ++x) {
            src.setPixel(x, y, qRgb(255, 255, 255));
        }
    }

    QImage norm = NormalGeneratorDialog::generateNormalMap(src, 2.0, 2, false, false);
    QCOMPARE(norm.size(), src.size());

    // Transparent pixels have neutral tangent-space normal (128, 128, 255) with alpha 0
    QRgb transPx = norm.pixel(0, 0);
    QCOMPARE(qAlpha(transPx), 0);
    QCOMPARE(qRed(transPx), 128);
    QCOMPARE(qGreen(transPx), 128);
    QCOMPARE(qBlue(transPx), 255);

    // Center flat pixel: near pure Z-up
    QRgb centerPx = norm.pixel(8, 8);
    QCOMPARE(qAlpha(centerPx), 255);
    QVERIFY(qBlue(centerPx) >= 240);
    QVERIFY(std::abs(qRed(centerPx) - 128) <= 5);
    QVERIFY(std::abs(qGreen(centerPx) - 128) <= 5);

    // Silhouette bevel edge has non-neutral normal
    QRgb edgePx = norm.pixel(4, 8);
    QCOMPARE(qAlpha(edgePx), 255);
    QVERIFY(qRed(edgePx) != 128);

    // Shaded preview rendering
    QImage shaded = NormalGeneratorDialog::renderShadedPreview(src, norm, 45.0, 55.0);
    QCOMPARE(shaded.size(), src.size());
    QCOMPARE(qAlpha(shaded.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(shaded.pixel(8, 8)), 255);
}

void TestFilters::testEmissiveGeneratorAlgorithm()
{
    QImage src(16, 16, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb darkBg = qRgb(30, 30, 30);
    QRgb brightNeon = qRgb(250, 250, 250);
    QRgb cyanMagic = qRgb(0, 240, 255);
    QRgb redBody = qRgb(200, 20, 20);

    src.setPixel(2, 2, darkBg);
    src.setPixel(5, 5, brightNeon);
    src.setPixel(8, 8, cyanMagic);
    src.setPixel(11, 11, redBody);

    // Mode 1: Luminance threshold (highlights)
    QImage emissiveLum = EmissiveGeneratorDialog::generateEmissiveMap(
        src, EmissiveGeneratorDialog::LuminanceThreshold, 180, 20, qRgb(0, 255, 255), 30, 1.5, false);

    // Dark pixel (lum ~30) is zeroed out
    QCOMPARE(qAlpha(emissiveLum.pixel(2, 2)), 0);
    // Bright neon (lum 250) is retained and boosted
    QCOMPARE(qAlpha(emissiveLum.pixel(5, 5)), 255);
    QVERIFY(qRed(emissiveLum.pixel(5, 5)) >= 250);

    // Mode 2: Color Key isolation (cyan magic)
    QImage emissiveColor = EmissiveGeneratorDialog::generateEmissiveMap(
        src, EmissiveGeneratorDialog::ColorKeyIsolation, 180, 20, qRgb(0, 255, 255), 30, 1.0, false);

    // Red body (hue 0 vs target cyan 180) is zeroed out
    QCOMPARE(qAlpha(emissiveColor.pixel(11, 11)), 0);
    // Cyan magic is retained
    QVERIFY(qAlpha(emissiveColor.pixel(8, 8)) > 150);
    QVERIFY(qBlue(emissiveColor.pixel(8, 8)) >= 200);
}

void TestFilters::testM21AuxiliaryMapUndoRedo()
{
    SpriteDocument doc;
    QImage frame1(16, 16, QImage::Format_ARGB32);
    frame1.fill(qRgb(255, 0, 0));
    QImage frame2(16, 16, QImage::Format_ARGB32);
    frame2.fill(qRgb(0, 255, 0));

    SpriteBox box1; box1.rect = QRect(0, 0, 16, 16);
    SpriteBox box2; box2.rect = QRect(16, 0, 16, 16);

    doc.setFrames({frame1, frame2}, {box1, box2});
    QCOMPARE(doc.frameCount(), 2);
    QVERIFY(!doc.hasAuxiliaryMap(MaterialMapType::Normal));

    QUndoStack undoStack;

    QList<QImage> normFrames = {
        NormalGeneratorDialog::generateNormalMap(frame1),
        NormalGeneratorDialog::generateNormalMap(frame2)
    };

    auto *cmd = new SetAuxiliaryMapCommand(&doc, MaterialMapType::Normal, normFrames);
    undoStack.push(cmd);

    QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Normal));
    QCOMPARE(doc.auxiliaryFrames(MaterialMapType::Normal).size(), 2);

    undoStack.undo();
    QVERIFY(!doc.hasAuxiliaryMap(MaterialMapType::Normal) || doc.auxiliaryFrames(MaterialMapType::Normal).isEmpty());

    undoStack.redo();
    QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Normal));
    QCOMPARE(doc.auxiliaryFrames(MaterialMapType::Normal).size(), 2);
}

void TestFilters::testAddMaterialLayerCommandUndoRedo()
{
    SpriteDocument doc;
    QImage frame1(16, 16, QImage::Format_ARGB32);
    frame1.fill(qRgb(255, 0, 0));
    QImage frame2(16, 16, QImage::Format_ARGB32);
    frame2.fill(qRgb(0, 255, 0));

    SpriteBox box1; box1.rect = QRect(0, 0, 16, 16);
    SpriteBox box2; box2.rect = QRect(16, 0, 16, 16);

    doc.setFrames({frame1, frame2}, {box1, box2});
    QCOMPARE(doc.frameCount(), 2);
    QVERIFY(!doc.hasLayers());
    QVERIFY(!doc.hasAuxiliaryMap(MaterialMapType::Normal));

    QUndoStack undoStack;

    QList<QImage> normFrames = {
        NormalGeneratorDialog::generateNormalMap(frame1),
        NormalGeneratorDialog::generateNormalMap(frame2)
    };

    auto *cmd = new AddMaterialLayerCommand(&doc, MaterialMapType::Normal, normFrames, QStringLiteral("Create Normal Layer"));
    undoStack.push(cmd);

    // After redo, document should have 2 layers: Albedo + Normal (_n)
    QVERIFY(doc.hasLayers());
    QCOMPARE(doc.layerCount(), 2);
    QCOMPARE(doc.layer(0).name, QStringLiteral("Albedo"));
    QCOMPARE(doc.layer(1).name, QStringLiteral("Normal (_n)"));
    QVERIFY(doc.hasFrameCels(0));
    QVERIFY(doc.hasFrameCels(1));
    QCOMPARE(doc.frameCels(0).size(), 2);
    QCOMPARE(doc.frameCels(1).size(), 2);
    QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Normal));
    QCOMPARE(doc.auxiliaryFrames(MaterialMapType::Normal).size(), 2);

    // Undo should restore document without layers
    undoStack.undo();
    QVERIFY(!doc.hasLayers());
    QVERIFY(!doc.hasAuxiliaryMap(MaterialMapType::Normal) || doc.auxiliaryFrames(MaterialMapType::Normal).isEmpty());

    // Redo should restore layers and cels
    undoStack.redo();
    QVERIFY(doc.hasLayers());
    QCOMPARE(doc.layerCount(), 2);
    QVERIFY(doc.hasAuxiliaryMap(MaterialMapType::Normal));
}

void TestFilters::testAtlasPackerCornersClampingAndExtrusion()
{
    QImage spr(10, 10, QImage::Format_ARGB32);
    spr.fill(qRgba(0, 0, 0, 0));

    QRgb body = qRgb(200, 100, 50);
    for (int y = 2; y <= 7; ++y) {
        for (int x = 2; x <= 7; ++x) {
            spr.setPixel(x, y, body);
        }
    }

    AtlasPacker::PackOptions opts;
    opts.extrude = 2;
    opts.padding = 6;
    opts.borderPadding = 4;

    AtlasPackResult res = AtlasPacker::pack({spr}, opts);
    QVERIFY(!res.atlas.isNull());
    QCOMPARE(res.frameRects.size(), 1);

    QRect placed = res.frameRects.first();
    QCOMPARE(res.atlas.pixel(placed.left() + 4, placed.top() + 4), body);

    // Direct applyExtrusion test to verify 8-connected diagonal corner clamping
    QImage canvas(24, 24, QImage::Format_ARGB32);
    canvas.fill(qRgba(0, 0, 0, 0));
    QRect target(6, 6, 10, 10);
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            canvas.setPixel(target.x() + x, target.y() + y, spr.pixel(x, y));
        }
    }
    AtlasPacker::applyExtrusion(canvas, target, spr, 2);

    // Check pixel at top-left diagonal corner extrusion
    QRgb diagExtruded = canvas.pixel(target.x() - 1, target.y() - 1);
    QCOMPARE(qAlpha(diagExtruded), 255);
    QCOMPARE(diagExtruded, body);
}

void TestFilters::testOutlineFilterAlgorithm()
{
    QImage src(12, 12, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb body = qRgb(200, 50, 50);
    QRgb blackOutline = qRgb(0, 0, 0);

    for (int y = 4; y <= 7; ++y) {
        for (int x = 4; x <= 7; ++x) {
            src.setPixel(x, y, body);
        }
    }

    // 1px 4-connected outline
    QImage res = OutlineFilterDialog::applyOutline(src, 1, blackOutline, OutlineFilterDialog::FourConnected, false);

    QCOMPARE(qAlpha(res.pixel(4, 3)), 255);
    QCOMPARE(qRed(res.pixel(4, 3)), 0);
    QCOMPARE(qAlpha(res.pixel(3, 4)), 255);
    QCOMPARE(qRed(res.pixel(3, 4)), 0);
    QCOMPARE(qAlpha(res.pixel(3, 3)), 0); // corner is 0 in 4-connected
    QCOMPARE(res.pixel(5, 5), body); // interior untouched

    // Silhouette mode
    QImage resSil = OutlineFilterDialog::applyOutline(src, 1, blackOutline, OutlineFilterDialog::FourConnected, true);
    QCOMPARE(qRed(resSil.pixel(5, 5)), 0);
}

void TestFilters::testColorSwapFilterAlgorithm()
{
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb greenBase = qRgb(0, 200, 0);
    QRgb redTarget = qRgb(220, 20, 20);

    src.setPixel(3, 3, greenBase);

    int modified = 0;
    QImage res = ColorSwapFilterDialog::applyColorSwap(src, greenBase, redTarget, 20, true, {}, &modified);
    QCOMPARE(modified, 1);

    QRgb swapped = res.pixel(3, 3);
    QCOMPARE(qAlpha(swapped), 255);
    QVERIFY(qRed(swapped) > 160);
    QVERIFY(qGreen(swapped) < 60);
}

void TestFilters::testColorAdjustFilterAlgorithm()
{
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    // Pure red pixel at (2, 2)
    src.setPixel(2, 2, qRgb(255, 0, 0));
    // Neutral gray pixel at (5, 5)
    src.setPixel(5, 5, qRgb(128, 128, 128));

    // 1. Hue Shift: +120° on pure red -> should become predominantly green
    QImage hueRes = ColorAdjustFilterDialog::applyColorAdjust(src, 120, 0, 0, 0);
    QRgb shiftedRed = hueRes.pixel(2, 2);
    QCOMPARE(qAlpha(shiftedRed), 255);
    QVERIFY(qGreen(shiftedRed) > 200);
    QVERIFY(qRed(shiftedRed) < 50);
    // Transparent pixel remains transparent
    QCOMPARE(qAlpha(hueRes.pixel(0, 0)), 0);

    // 2. Saturation: -100% on pure red -> should become grayscale (R == G == B)
    QImage desatRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, -100, 0, 0);
    QRgb grayPix = desatRes.pixel(2, 2);
    QCOMPARE(qAlpha(grayPix), 255);
    QCOMPARE(qRed(grayPix), qGreen(grayPix));
    QCOMPARE(qGreen(grayPix), qBlue(grayPix));

    // 3. Brightness/Value: -100% -> should become completely black
    QImage darkRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, 0, -100, 0);
    QRgb darkPix = darkRes.pixel(2, 2);
    QCOMPARE(qAlpha(darkPix), 255);
    QCOMPARE(qRed(darkPix), 0);
    QCOMPARE(qGreen(darkPix), 0);
    QCOMPARE(qBlue(darkPix), 0);

    // 4. Target Areas scope test: only pixels inside the rect are altered
    QList<QRect> scope = { QRect(1, 1, 3, 3) }; // covers (2,2), not (5,5)
    QImage scopedRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, 0, -100, 0, scope);
    QCOMPARE(qRed(scopedRes.pixel(2, 2)), 0); // modified
    QCOMPARE(qRed(scopedRes.pixel(5, 5)), 128); // untouched
}

void TestFilters::testPixelRescaleFilterAlgorithm()
{
    // Create a 4x4 test pattern
    QImage src(4, 4, QImage::Format_ARGB32);
    src.fill(qRgb(255, 255, 255));
    src.setPixel(1, 1, qRgb(255, 0, 0));
    src.setPixel(2, 2, qRgb(0, 0, 255));

    // 1. Nearest 2x -> 8x8 image
    QImage near2x = PixelRescaleFilterDialog::applyNearest(src, 2.0);
    QCOMPARE(near2x.width(), 8);
    QCOMPARE(near2x.height(), 8);
    QCOMPARE(near2x.pixel(2, 2), qRgb(255, 0, 0));
    QCOMPARE(near2x.pixel(3, 3), qRgb(255, 0, 0));
    QCOMPARE(near2x.pixel(4, 4), qRgb(0, 0, 255));
    QCOMPARE(near2x.pixel(0, 0), qRgb(255, 255, 255));

    // 2. Nearest 0.5x -> 2x2 image
    QImage nearHalf = PixelRescaleFilterDialog::applyNearest(src, 0.5);
    QCOMPARE(nearHalf.width(), 2);
    QCOMPARE(nearHalf.height(), 2);

    // 3. Scale2x -> 8x8 image
    QImage scale2x = PixelRescaleFilterDialog::applyScale2x(src);
    QCOMPARE(scale2x.width(), 8);
    QCOMPARE(scale2x.height(), 8);
    QVERIFY(!scale2x.isNull());

    // 4. Scale3x -> 12x12 image
    QImage scale3x = PixelRescaleFilterDialog::applyScale3x(src);
    QCOMPARE(scale3x.width(), 12);
    QCOMPARE(scale3x.height(), 12);

    // 5. Test Bounding Box proportional rescaling in dialog
    SpriteDocument doc;
    doc.setAtlas(src);
    SpriteBox b0;
    b0.rect = QRect(1, 1, 2, 2);
    b0.index = 0;
    doc.setFrames({ src.copy(b0.rect) }, { b0 });

    QUndoStack stack;
    PixelRescaleFilterDialog dlg(&doc, &stack);
    // Auto-detect disabled: bounding box is scaled mathematically
    dlg.setAutoDetectBoxesEnabled(false);
    // Dialog accepts default 2x scale
    dlg.accept();

    QCOMPARE(doc.atlas().width(), 8);
    QCOMPARE(doc.atlas().height(), 8);
    QCOMPARE(doc.boxes().size(), 1);
    QCOMPARE(doc.boxes().first().rect, QRect(2, 2, 4, 4));

    // Undo restores original atlas and box
    stack.undo();
    QCOMPARE(doc.atlas().width(), 4);
    QCOMPARE(doc.atlas().height(), 4);
    QCOMPARE(doc.boxes().first().rect, QRect(1, 1, 2, 2));
}

void TestFilters::testRetroPaletteFilterAlgorithm()
{
    // 1. Built-in Preset verification via dynamic ID
    QVector<QRgb> dmg = RetroPaletteFilterDialog::getPresetPalette(QStringLiteral("gameboy_dmg"));
    QCOMPARE(dmg.size(), 4);

    QVector<QRgb> pico8 = RetroPaletteFilterDialog::getPresetPalette(QStringLiteral("pico8"));
    QCOMPARE(pico8.size(), 16);

    QVector<QRgb> nes = RetroPaletteFilterDialog::getPresetPalette(QStringLiteral("nes"));
    QCOMPARE(nes.size(), 54);

    QVector<QRgb> endesga = RetroPaletteFilterDialog::getPresetPalette(QStringLiteral("endesga32"));
    QCOMPARE(endesga.size(), 32);

    // 2. Nearest Quantization without dithering
    QImage src(4, 4, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));
    src.setPixel(0, 0, qRgb(0, 0, 0));       // Pure black -> Darkest DMG green (15, 56, 15)
    src.setPixel(1, 1, qRgb(255, 255, 255)); // Pure white -> Lightest DMG green (155, 188, 15)

    QImage quantNone = RetroPaletteFilterDialog::applyRetroPalette(
        src, dmg, RetroPaletteFilterDialog::DitherNone, 0);

    QCOMPARE(qAlpha(quantNone.pixel(0, 0)), 255);
    QCOMPARE(quantNone.pixel(0, 0), dmg[0]); // darkest green
    QCOMPARE(quantNone.pixel(1, 1), dmg[3]); // lightest green
    QCOMPARE(qAlpha(quantNone.pixel(2, 2)), 0); // transparent pixel untouched

    // 3. Ordered Bayer Dithering
    QImage ditherSrc(8, 8, QImage::Format_ARGB32);
    // Fill with intermediate gray to trigger spatial alternating pattern
    ditherSrc.fill(qRgb(100, 120, 50));
    QImage ditherRes = RetroPaletteFilterDialog::applyRetroPalette(
        ditherSrc, dmg, RetroPaletteFilterDialog::Bayer4x4, 50);

    // Verify dithering generated spatial variation (multiple palette tones) across uniform input
    QSet<QRgb> uniqueDitherColors;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            uniqueDitherColors.insert(ditherRes.pixel(x, y));
        }
    }
    QVERIFY(uniqueDitherColors.size() >= 2);

    // 4. Custom Palette Parser test
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString hexFile = tempDir.filePath(QStringLiteral("test_palette.hex"));
    {
        QFile f(hexFile);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "#ff0000\n";
        out << "00ff00\n";
        out << "#0000ff\n";
    }

    QString parseErr;
    QVector<QRgb> parsed = RetroPaletteFilterDialog::loadPaletteFromFile(hexFile, &parseErr);
    QVERIFY(parseErr.isEmpty());
    QCOMPARE(parsed.size(), 3);
    QCOMPARE(parsed[0], qRgb(255, 0, 0));
    QCOMPARE(parsed[1], qRgb(0, 255, 0));
    QCOMPARE(parsed[2], qRgb(0, 0, 255));
}

void TestFilters::testApplyFilterCommandUndoRedo()
{
    SpriteDocument doc;
    QImage img1(10, 10, QImage::Format_ARGB32);
    img1.fill(qRgb(255, 255, 255));
    doc.setAtlas(img1);

    SpriteBox b1;
    b1.rect = QRect(0, 0, 10, 10);
    b1.index = 0;
    doc.setFrames({ img1 }, { b1 });

    QImage img2(10, 10, QImage::Format_ARGB32);
    img2.fill(qRgb(0, 0, 255));

    QUndoStack stack;
    stack.push(new ApplyFilterCommand(&doc, QStringLiteral("Blue Filter"),
                                      img1, doc.frames(), doc.boxes(), doc.animations(),
                                      img2, { img2 }, { b1 }, doc.animations()));

    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 0);

    stack.undo();
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);

    stack.redo();
    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 0);
}

void TestFilters::testComputeDirtyRect()
{
    // 1. Identical images -> empty dirty rect
    QImage img1(64, 64, QImage::Format_ARGB32);
    img1.fill(qRgb(10, 20, 30));
    QImage img2 = img1.copy();
    QVERIFY(PixelHash::computeDirtyRect(img1, img2).isEmpty());

    // 2. Localized rectangular patch modification
    // Modify pixels from (15, 10) to (34, 29) -> width 20, height 20
    for (int y = 10; y < 30; ++y) {
        for (int x = 15; x < 35; ++x) {
            img2.setPixel(x, y, qRgb(200, 150, 100));
        }
    }
    QRect dirty = PixelHash::computeDirtyRect(img1, img2);
    QCOMPARE(dirty, QRect(15, 10, 20, 20));

    // 3. Null or mismatched sizes
    QImage img3(32, 32, QImage::Format_ARGB32);
    QRect fullBound = PixelHash::computeDirtyRect(img1, img3);
    QCOMPARE(fullBound, QRect(0, 0, 64, 64));
    QVERIFY(PixelHash::computeDirtyRect(QImage(), img1).isValid());
}

void TestFilters::testDifferentialAtlasFilterUndoRedo()
{
    // Test large atlas differential undo snapshotting (Chantier M14 - Palier 1)
    SpriteDocument doc;
    // Create an atlas (128x128) representing a high-res master atlas
    QImage initialAtlas(128, 128, QImage::Format_ARGB32);
    initialAtlas.fill(qRgb(10, 10, 10)); // Dark grey background
    doc.setAtlas(initialAtlas);

    SpriteBox box;
    box.rect = QRect(20, 25, 16, 16);
    box.index = 0;
    QImage spriteFrame(16, 16, QImage::Format_ARGB32);
    spriteFrame.fill(qRgb(255, 0, 0)); // Red sprite
    doc.setFrames({ spriteFrame }, { box });

    // Composite initial red sprite onto atlas
    doc.patchAtlasDirect(box.rect, spriteFrame);
    QCOMPARE(doc.atlas().pixel(25, 30), qRgb(255, 0, 0));
    QCOMPARE(doc.atlas().pixel(0, 0), qRgb(10, 10, 10));

    // Simulate filter applying to that specific sprite (turning it Green)
    QImage filteredAtlas = doc.atlas().copy();
    QImage filteredFrame = spriteFrame.copy();
    filteredFrame.fill(qRgb(0, 255, 0)); // Green sprite
    {
        QPainter p(&filteredAtlas);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.drawImage(box.rect.topLeft(), filteredFrame);
    }

    QUndoStack stack;
    auto *cmd = new ApplyFilterCommand(&doc, QStringLiteral("Recolor Sprite"),
                                       doc.atlas(), doc.frames(), doc.boxes(), doc.animations(),
                                       filteredAtlas, { filteredFrame }, { box }, doc.animations());
    stack.push(cmd);

    // After redo / push: sprite should be green, surroundings untouched
    QCOMPARE(doc.atlas().pixel(25, 30), qRgb(0, 255, 0));
    QCOMPARE(doc.atlas().pixel(0, 0), qRgb(10, 10, 10));
    QCOMPARE(doc.atlas().pixel(100, 100), qRgb(10, 10, 10));

    // After undo: sprite should be restored to red, surroundings untouched
    stack.undo();
    QCOMPARE(doc.atlas().pixel(25, 30), qRgb(255, 0, 0));
    QCOMPARE(doc.atlas().pixel(0, 0), qRgb(10, 10, 10));
    QCOMPARE(doc.atlas().pixel(100, 100), qRgb(10, 10, 10));

    // After redo: sprite should be green again
    stack.redo();
    QCOMPARE(doc.atlas().pixel(25, 30), qRgb(0, 255, 0));
    QCOMPARE(doc.atlas().pixel(0, 0), qRgb(10, 10, 10));
}

void TestFilters::testCpuFeatures()
{
    // Test CPU feature query and architecture identification (Chantier M14 - Palier 2)
    const char *archName = CpuFeatures::simdArchitectureName();
    QVERIFY(archName != nullptr);
    QVERIFY(std::strlen(archName) > 0);

#if defined(BENTOPACK_HAS_AVX2)
    // On our x86_64 build host with AVX2, this must detect AVX2 support
    QVERIFY(CpuFeatures::hasAvx2());
    QVERIFY(std::string(archName).find("AVX2") != std::string::npos);
#endif
}

void TestFilters::testSimdColorSwapRowAccuracy()
{
    // Test bit-for-bit exactness between SIMD kernel and scalar baseline
    // Test across various row lengths to stress exact block sizes, non-multiples, and tails
    const QList<int> testCounts = { 1, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 64, 127, 256 };

    const QRgb srcTarget = qRgb(0, 200, 0);       // Green target
    const QRgb dstTarget = qRgb(220, 20, 20);     // Red replacement
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

    for (int count : testCounts) {
        QVector<QRgb> src(count);
        QVector<QRgb> dstScalar(count);
        QVector<QRgb> dstSimd(count);

        // Populate row with diverse pixels (exact match, near match, distant, transparent)
        for (int i = 0; i < count; ++i) {
            switch (i % 6) {
            case 0:
                src[i] = qRgba(0, 200, 0, 255);       // Exact match
                break;
            case 1:
                src[i] = qRgba(10, 195, 5, 255);      // Near match (within tolerance)
                break;
            case 2:
                src[i] = qRgba(10, 195, 5, 0);        // Near match but transparent (should not swap)
                break;
            case 3:
                src[i] = qRgba(0, 0, 250, 255);       // Distant (Blue)
                break;
            case 4:
                src[i] = qRgba(250, 250, 250, 255);   // White
                break;
            case 5:
                src[i] = qRgba(0, 210, 10, 128);      // Near match with semi-transparency
                break;
            }
            dstScalar[i] = src[i];
            dstSimd[i] = src[i];
        }

        // 1. Test direct replacement mode (preserveShading = false)
        int modScalar = SimdKernels::colorSwapRow_scalar(src.constData(), dstScalar.data(), count,
                                                         qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                                         maxDistSq, dstRgbBase, false,
                                                         sV, sS, dH, dS, dV);

        int modSimd = SimdKernels::colorSwapRow(src.constData(), dstSimd.data(), count,
                                               qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                               maxDistSq, dstRgbBase, false,
                                               sV, sS, dH, dS, dV);

        QCOMPARE(modSimd, modScalar);
        for (int i = 0; i < count; ++i) {
            QCOMPARE(dstSimd[i], dstScalar[i]);
        }

        // 2. Test shading preservation mode (preserveShading = true)
        dstScalar = src;
        dstSimd = src;
        int modScalarShade = SimdKernels::colorSwapRow_scalar(src.constData(), dstScalar.data(), count,
                                                              qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                                              maxDistSq, dstRgbBase, true,
                                                              sV, sS, dH, dS, dV);

        int modSimdShade = SimdKernels::colorSwapRow(src.constData(), dstSimd.data(), count,
                                                    qRed(srcTarget), qGreen(srcTarget), qBlue(srcTarget),
                                                    maxDistSq, dstRgbBase, true,
                                                    sV, sS, dH, dS, dV);

        QCOMPARE(modSimdShade, modScalarShade);
        for (int i = 0; i < count; ++i) {
            QCOMPARE(dstSimd[i], dstScalar[i]);
        }
    }
}

void TestFilters::testSimdDespillChromaRowAccuracy()
{
    const QList<int> testCounts = { 1, 4, 7, 8, 16, 23, 64 };

    for (int count : testCounts) {
        QVector<QRgb> src(count);
        QVector<QRgb> dstScalar(count);
        QVector<QRgb> dstSimd(count);

        for (int i = 0; i < count; ++i) {
            if (i % 3 == 0) {
                src[i] = qRgba(30, 210, 40, 255);   // Strong green spill
            } else if (i % 3 == 1) {
                src[i] = qRgba(180, 50, 60, 255);   // Red dominant (no spill)
            } else {
                src[i] = qRgba(20, 220, 30, 0);     // Transparent (ignored)
            }
            dstScalar[i] = src[i];
            dstSimd[i] = src[i];
        }

        int modScalar = SimdKernels::despillChromaRow_scalar(src.constData(), dstScalar.data(), count,
                                                            true, false, 1.0);
        int modSimd = SimdKernels::despillChromaRow(src.constData(), dstSimd.data(), count,
                                                   true, false, 1.0);

        QCOMPARE(modSimd, modScalar);
        for (int i = 0; i < count; ++i) {
            QCOMPARE(dstSimd[i], dstScalar[i]);
        }
    }
}

void TestFilters::testFilterAutoDetectBoxes()
{
    SpriteDocument doc;
    QImage baseImg(32, 32, QImage::Format_ARGB32);
    baseImg.fill(qRgba(0, 0, 0, 0));

    // Draw an 8x8 white square in the center (from 12,12 to 19,19)
    for (int y = 12; y < 20; ++y) {
        for (int x = 12; x < 20; ++x) {
            baseImg.setPixel(x, y, qRgb(255, 255, 255));
        }
    }
    doc.setAtlas(baseImg);

    SpriteBox initialBox;
    initialBox.rect = QRect(12, 12, 8, 8);
    initialBox.index = 0;
    doc.setFrames({ baseImg.copy(initialBox.rect) }, { initialBox });

    // 1. Test OutlineFilterDialog with auto-detect enabled (default for outline)
    {
        QUndoStack stack;
        OutlineFilterDialog dlg(&doc, &stack);
        QCOMPARE(dlg.isAutoDetectBoxesEnabled(), true);

        // Accept the dialog to commit
        dlg.accept();

        QVERIFY(doc.boxes().size() >= 1);
        QRect detectedRect = doc.boxes().first().rect;
        // Outline thickness is 1 by default (or 1px expanded: 12-1=11 to 19+1=20 => 10x10)
        QCOMPARE(detectedRect, QRect(11, 11, 10, 10));
    }

    // 2. Test with auto-detect disabled
    {
        // Reset document to initial state
        doc.setAtlas(baseImg);
        doc.setFrames({ baseImg.copy(initialBox.rect) }, { initialBox });

        QUndoStack stack;
        OutlineFilterDialog dlg(&doc, &stack);
        dlg.setAutoDetectBoxesEnabled(false);
        QCOMPARE(dlg.isAutoDetectBoxesEnabled(), false);

        dlg.accept();

        // When auto-detect is disabled, the initial box rect is preserved
        QCOMPARE(doc.boxes().size(), 1);
        QCOMPARE(doc.boxes().first().rect, QRect(12, 12, 8, 8));
    }

    // 3. Test Despill, ColorSwap, ColorAdjust, PixelRescale, RetroPalette have auto-detect disabled by default
    {
        QUndoStack stack;
        DespillFilterDialog despillDlg(&doc, &stack);
        QCOMPARE(despillDlg.isAutoDetectBoxesEnabled(), false);
        despillDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(despillDlg.isAutoDetectBoxesEnabled(), true);

        ColorSwapFilterDialog swapDlg(&doc, &stack);
        QCOMPARE(swapDlg.isAutoDetectBoxesEnabled(), false);
        swapDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(swapDlg.isAutoDetectBoxesEnabled(), true);

        ColorAdjustFilterDialog adjustDlg(&doc, &stack);
        QCOMPARE(adjustDlg.isAutoDetectBoxesEnabled(), false);
        adjustDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(adjustDlg.isAutoDetectBoxesEnabled(), true);

        PixelRescaleFilterDialog rescaleDlg(&doc, &stack);
        QCOMPARE(rescaleDlg.isAutoDetectBoxesEnabled(), false);
        rescaleDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(rescaleDlg.isAutoDetectBoxesEnabled(), true);

        RetroPaletteFilterDialog retroDlg(&doc, &stack);
        QCOMPARE(retroDlg.isAutoDetectBoxesEnabled(), false);
        retroDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(retroDlg.isAutoDetectBoxesEnabled(), true);
    }
}

void TestFilters::testAtlasPackingFilterInteractive()
{
    SpriteDocument doc;
    QUndoStack stack;

    // Create 4 frames:
    // Frame 0: 20x20 red
    // Frame 1: 30x20 green
    // Frame 2: 20x20 red (identical to frame 0)
    // Frame 3: 25x25 blue
    QImage red(20, 20, QImage::Format_ARGB32);
    red.fill(Qt::red);

    QImage green(30, 20, QImage::Format_ARGB32);
    green.fill(Qt::green);

    QImage blue(25, 25, QImage::Format_ARGB32);
    blue.fill(Qt::blue);

    QList<QImage> initialFrames = { red, green, red, blue };

    // Arrange in a naive 200x200 atlas
    QImage initialAtlas(200, 200, QImage::Format_ARGB32);
    initialAtlas.fill(Qt::transparent);

    QList<SpriteBox> initialBoxes;
    initialBoxes.reserve(4);
    for (int i = 0; i < 4; ++i) {
        SpriteBox b;
        b.rect = QRect(i * 35, 0, initialFrames[i].width(), initialFrames[i].height());
        b.index = i;
        b.pivot = QPoint(5, 5);
        b.hasCustomPivot = true;
        initialBoxes.append(b);
    }

    doc.setAtlas(initialAtlas);
    doc.setFrames(initialFrames, initialBoxes);

    // Create an animation referencing frames [0, 1, 2, 3, 2, 1]
    doc.setAnimation(QStringLiteral("walk"), {0, 1, 2, 3, 2, 1}, 12, true, SpriteAnimation::Loop);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2, 3, 2, 1}));

    // 1. Instantiate dialog with Deduplication ENABLED
    AtlasPackingDialog dlg(&doc, &stack);
    dlg.setDeduplicate(true);

    // Call accept() to apply and create undo command
    dlg.accept();

    // Verification:
    // With deduplication, unique frames count is 3 (frame 2 merged into frame 0)
    QTRY_COMPARE(doc.frameCount(), 3);
    QTRY_COMPARE(doc.boxes().size(), 3);

    // Animation frames must be remapped seamlessly:
    // Frame 0 -> 0
    // Frame 1 -> 1
    // Frame 2 -> 0 (merged with 0)
    // Frame 3 -> 2
    // Expected sequence: [0, 1, 0, 2, 0, 1]
    SpriteAnimation packedAnim = doc.animation(QStringLiteral("walk"));
    QCOMPARE(packedAnim.frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));
    QCOMPARE(packedAnim.fps, 12);
    QCOMPARE(packedAnim.loopMode, SpriteAnimation::Loop);

    // Bounding boxes must not overlap in the packed atlas
    for (int i = 0; i < doc.boxes().size(); ++i) {
        QVERIFY(doc.atlas().rect().contains(doc.boxes()[i].rect));
        for (int j = i + 1; j < doc.boxes().size(); ++j) {
            QVERIFY(!doc.boxes()[i].rect.intersects(doc.boxes()[j].rect));
        }
    }

    // 2. Undo test: Ctrl+Z must restore 4 frames and original animation [0, 1, 2, 3, 2, 1]
    stack.undo();
    QCOMPARE(doc.frameCount(), 4);
    QCOMPARE(doc.boxes().size(), 4);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2, 3, 2, 1}));
    QCOMPARE(doc.atlas().size(), QSize(200, 200));

    // 3. Redo test: Ctrl+Y reapplies packing and remapped animation
    stack.redo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));

    // 4. Reject / Cancel test:
    AtlasPackingDialog dlgCancel(&doc, &stack);
    dlgCancel.reject();
    // Verify document remains in the 3-frame packed state without corruption
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));
}

void TestFilters::testSpriteDetectionFilterInteractive()
{
    SpriteDocument doc;
    QUndoStack stack;

    // Create an atlas that ALREADY has a transparent background (0, 0, 0, 0)
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(qRgba(0, 0, 0, 0));

    // Draw two opaque sprite rectangles
    // Sprite 1: (10, 10, 20, 20) in red
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 30; ++x) {
            atlas.setPixel(x, y, qRgb(255, 0, 0));
        }
    }
    // Sprite 2: (60, 20, 25, 25) in blue
    for (int y = 20; y < 45; ++y) {
        for (int x = 60; x < 85; ++x) {
            atlas.setPixel(x, y, qRgb(0, 0, 255));
        }
    }

    doc.setAtlas(atlas);
    QCOMPARE(doc.frameCount(), 0);

    // 1. Direct Sprite Detection without background removal
    SpriteDetectionDialog dlg(&doc, &stack);
    QVERIFY(dlg.isSmartCropEnabled());
    QCOMPARE(dlg.alphaThreshold(), AppConfig::instance().atlas().defaultAlphaThreshold);

    dlg.accept();

    // Verify 2 frames detected
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.boxes().size(), 2);

    // Verify atlas pixels were NOT touched or corrupted
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 0);
    QCOMPARE(doc.atlas().pixel(10, 10), qRgb(255, 0, 0));
    QCOMPARE(doc.atlas().pixel(60, 20), qRgb(0, 0, 255));

    // Verify bounding boxes wrap the sprites correctly
    QRect r1 = doc.boxes()[0].rect;
    QRect r2 = doc.boxes()[1].rect;
    QVERIFY(r1.contains(QPoint(10, 10)) || r2.contains(QPoint(10, 10)));
    QVERIFY(r1.contains(QPoint(60, 20)) || r2.contains(QPoint(60, 20)));

    // 2. Undo & Redo
    stack.undo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(doc.boxes().size(), 0);

    stack.redo();
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.boxes().size(), 2);
}

void TestFilters::testAboutDialogDarkModeAndPricing()
{
    // Test instantiation with a dark palette (simulating dark theme)
    QPalette origPalette = qApp->palette();
    QPalette darkPalette = origPalette;
    darkPalette.setColor(QPalette::Window, QColor("#181920"));
    darkPalette.setColor(QPalette::WindowText, QColor("#e2e8f0"));
    darkPalette.setColor(QPalette::Base, QColor("#1f222d"));
    darkPalette.setColor(QPalette::Text, QColor("#e2e8f0"));
    darkPalette.setColor(QPalette::Button, QColor("#262936"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#e2e8f0"));
    qApp->setPalette(darkPalette);

    LocalizationManager::instance().setLanguage(QStringLiteral("fr_FR"));

    AboutDialog dlg;

    QTabWidget *tabs = dlg.findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 5);

    // Verify Tab names
    QStringList tabNames;
    for (int i = 0; i < tabs->count(); ++i) {
        tabNames << tabs->tabText(i);
    }
    QVERIFY(tabNames.contains(QStringLiteral("KEY_DIALOG_ABOUT_PRICING")) || tabNames.contains(QStringLiteral("Tarifs && Licences")));
    QVERIFY(tabNames.contains(QStringLiteral("KEY_DIALOG_ABOUT_PLUGINS")) || tabNames.contains(QStringLiteral("Plugins && Statut")));

    // Verify pricing content
    tabs->setCurrentIndex(1); // Tarifs & Licences
    QTextEdit *pricingEditor = qobject_cast<QTextEdit*>(tabs->currentWidget());
    QVERIFY(pricingEditor != nullptr);
    QString pricingHtml = pricingEditor->toHtml();
    QVERIFY(pricingHtml.contains("Open Source") || pricingHtml.contains("Apache"));
    QVERIFY(pricingHtml.contains("0"));
    QVERIFY(pricingHtml.contains("19"));

    // Verify plugins info
    tabs->setCurrentIndex(2); // Plugins & Statut
    QTextEdit *pluginsEditor = qobject_cast<QTextEdit*>(tabs->currentWidget());
    QVERIFY(pluginsEditor != nullptr);
    QString pluginsHtml = pluginsEditor->toHtml();
    QVERIFY(pluginsHtml.contains("BentoPackCore"));
    QVERIFY(pluginsHtml.contains("Apache 2.0") || pluginsHtml.contains("KEY_PLUGINS_CORE_DESC"));

    // Verify dark palette doesn't crash and renders properly
    dlg.resize(700, 580);
    dlg.adjustSize();
    QImage imgDark(dlg.size(), QImage::Format_ARGB32_Premultiplied);
    imgDark.fill(Qt::transparent);
    dlg.render(&imgDark);
    QVERIFY(!imgDark.isNull());
    QVERIFY(imgDark.width() > 0 && imgDark.height() > 0);

    // Grab pricing tab
    tabs->setCurrentIndex(1);
    QImage imgPricing(dlg.size(), QImage::Format_ARGB32_Premultiplied);
    imgPricing.fill(Qt::transparent);
    dlg.render(&imgPricing);
    QVERIFY(!imgPricing.isNull());

    // Grab plugins tab
    tabs->setCurrentIndex(2);
    QImage imgPlugins(dlg.size(), QImage::Format_ARGB32_Premultiplied);
    imgPlugins.fill(Qt::transparent);
    dlg.render(&imgPlugins);
    QVERIFY(!imgPlugins.isNull());

    // Verify Licence tab contains Apache 2.0
    tabs->setCurrentIndex(4); // Licence
    QTextEdit *licenseEditor = qobject_cast<QTextEdit*>(tabs->currentWidget());
    QVERIFY(licenseEditor != nullptr);
    QString licenseHtml = licenseEditor->toHtml();
    QVERIFY(licenseHtml.contains("Apache License") || licenseHtml.contains("Apache 2.0") || licenseHtml.contains("KEY_LICENSE_CORE_TITLE"));

    // Grab license tab
    QImage imgLicense(dlg.size(), QImage::Format_ARGB32_Premultiplied);
    imgLicense.fill(Qt::transparent);
    dlg.render(&imgLicense);
    QVERIFY(!imgLicense.isNull());

    // Test dynamic multilingual pricing loading in French
    LocalizationManager::instance().setLanguage(QStringLiteral("fr_FR"));
    AboutDialog dlgFr;
    QTabWidget *tabsFr = dlgFr.findChild<QTabWidget*>();
    QVERIFY(tabsFr != nullptr);
    tabsFr->setCurrentIndex(1);
    QTextEdit *pricingFr = qobject_cast<QTextEdit*>(tabsFr->currentWidget());
    QVERIFY(pricingFr != nullptr);
    QString htmlFr = pricingFr->toHtml();
    QVERIFY(htmlFr.contains("0 €"));
    QVERIFY(htmlFr.contains("19 €"));
    QVERIFY(htmlFr.contains("Gratuit") || htmlFr.contains("Apache"));
    QVERIFY(htmlFr.contains("Pour qui :"));

    // Test dynamic multilingual pricing loading in English
    LocalizationManager::instance().setLanguage(QStringLiteral("en_US"));
    AboutDialog dlgEn;
    QTabWidget *tabsEn = dlgEn.findChild<QTabWidget*>();
    QVERIFY(tabsEn != nullptr);
    tabsEn->setCurrentIndex(1);
    QTextEdit *pricingEn = qobject_cast<QTextEdit*>(tabsEn->currentWidget());
    QVERIFY(pricingEn != nullptr);
    QString htmlEn = pricingEn->toHtml();
    QVERIFY(htmlEn.contains("$0"));
    QVERIFY(htmlEn.contains("$19"));
    QVERIFY(htmlEn.contains("Free") || htmlEn.contains("Apache"));
    QVERIFY(htmlEn.contains("Target:"));

    // Test dynamic multilingual pricing loading in Japanese
    LocalizationManager::instance().setLanguage(QStringLiteral("ja_JP"));
    AboutDialog dlgJa;
    QTabWidget *tabsJa = dlgJa.findChild<QTabWidget*>();
    QVERIFY(tabsJa != nullptr);
    tabsJa->setCurrentIndex(1);
    QTextEdit *pricingJa = qobject_cast<QTextEdit*>(tabsJa->currentWidget());
    QVERIFY(pricingJa != nullptr);
    QString htmlJa = pricingJa->toHtml();
    QVERIFY(htmlJa.contains("0 円"));
    QVERIFY(htmlJa.contains("1 980 円") || htmlJa.contains("1,980 円") || htmlJa.contains("1980"));
    QVERIFY(htmlJa.contains("無料") || htmlJa.contains("Apache"));
    QVERIFY(htmlJa.contains("対象:"));

    qApp->setPalette(origPalette);
}

int main(int argc, char *argv[])
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    QApplication app(argc, argv);
    TestFilters tf;

    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    if (!args.contains(QStringLiteral("-o"))) {
        args << QStringLiteral("-o") << QStringLiteral("-,txt");
    }

    int result = QTest::qExec(&tf, args);
    std::fflush(stdout);
    std::fflush(stderr);
    return result;
}

#include "test_filters.moc"
