#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QUndoStack>
#include <QApplication>
#include <QDir>

#include "model/spritedocument.h"
#include "commands/commands.h"
#include "geometry/triangulator.h"
#include "widgets/pixelcanvas.h"
#include "widgets/pixeleditordialog.h"
#include "image/colorpalettepresets.h"
#include "widgets/colorpickerwidget.h"
#include "filters/filterregistry.h"
#include "filters/filterplugin.h"
#include "widgets/filterdialogbase.h"
#include "widgets/layerstackwidget.h"
#include <QMenu>
#include <QTimer>

class TestPixelEditor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 1. EditSpritePixelsCommand Tests
    void testEditSpritePixelsCommandSingleFrame();
    void testEditSpritePixelsCommandMultiFrame();
    void testEditSpritePixelsCommandUndoRedoAtlasIntegrity();

    // 2. PixelCanvas Drawing & Bresenham Tests
    void testPencilBresenhamHorizontal();
    void testPencilBresenhamDiagonalContinuous();
    void testEraserClearsAlpha();

    // 3. Flood Fill (Bucket) Tests
    void testBucketFillSolid();
    void testBucketFillBoundedBySelection();

    // 4. Selections & Clipboard Tests
    void testRectangularSelection();
    void testColorSelectionMagicWand();
    void testClearSelection();
    void testCopyCutPasteFloating();

    // 5. Transformations Tests
    void testFlipHorizontal();
    void testFlipVertical();
    void testRotate90CW();

    // 6. Palettes Tests
    void testRetroPalettesAuthenticity();
    void testDynamicSpriteColorExtraction();

    // 7. Dirty Rects & COW Tests
    void testPixelCanvasDirtyRectUndoRedo();

    // 8. Onion Skinning & Ergonomics Tests
    void testOnionSkinTintedAndFalloff();
    void testOnionSkinEdgeDetection();
    void testOnionSkinChannelsAndSilhouette();
    void testOnionSkinCanvasComposition();
    void testOnionSkinPivotAlignment();
    void testOnionSkinPolygonClipping();
    void testSpaceAndMiddleMousePanning();
    void testAnimationScopedNavigation();
    void testZoomPreservedAcrossFrameNavigation();
    void testPivotStrictlyStationaryAcrossAnimation();
    void testPolygonRestrictionCheckboxAndEditing();
    void testPolygonFollowsFlipAndRotate();
    void testAtlasPolygonCollisionAndRepack();
    void testApplyToAllFramesRelativePivot();
    void testProColorPickerAndHarmonies();
    void testPixelEditorNonAtlasFiltersAndAnimationScope();
    void testPixelEditorFilterLivePreviewAndRollback();

    // 9. Multi-Layer Editing & Layer Stack Tests (M18)
    void testLayerStackWidgetAndControls();
    void testMultiLayerCanvasDrawingAndLock();
    void testMultiLayerBlendModesAndOpacity();
    void testMultiLayerSamplingAndFloodFill();
    void testMultiLayerOnionSkinRestricted();
    void testMultiLayerSessionSaveAndDocumentSync();
    void testErgonomicContextualPanels();
    void testFilterAllFramesMultiLayerAndNavigation();

    // 10. Intelligent Polygon Mesh Interactive Editing (M19)
    void testPixelEditorPolygonMeshInteractiveEditingAndUndo();
    void testSmartMeshGenerationApplyToAllAnimationFrames();
};

void TestPixelEditor::initTestCase()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString binPlugins = QDir(appDir).filePath(QStringLiteral("plugins"));
    FilterRegistry::instance().loadPlugins(binPlugins);
    FilterRegistry::instance().loadPlugins(appDir);
    FilterRegistry::instance().initDefaultFilters();
}

void TestPixelEditor::cleanupTestCase()
{
}

void TestPixelEditor::testEditSpritePixelsCommandSingleFrame()
{
    SpriteDocument doc;
    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::blue);
    doc.setAtlas(atlas);

    QImage frame(32, 32, QImage::Format_ARGB32);
    frame.fill(Qt::blue);
    SpriteBox box(QRect(0, 0, 32, 32));
    doc.addFrame(frame, box);

    QUndoStack undoStack;

    // Modify frame 0 with red pixels
    QImage newFrame = frame.copy();
    newFrame.fill(Qt::red);

    EditSpritePixelsCommand *cmd = new EditSpritePixelsCommand(&doc, 0, newFrame);
    undoStack.push(cmd);

    // Frame and Atlas should now be red in that 32x32 area
    QCOMPARE(doc.frame(0).pixelColor(10, 10), QColor(Qt::red));
    QCOMPARE(doc.atlas().pixelColor(10, 10), QColor(Qt::red));

    // Undo: should revert to blue
    undoStack.undo();
    QCOMPARE(doc.frame(0).pixelColor(10, 10), QColor(Qt::blue));
    QCOMPARE(doc.atlas().pixelColor(10, 10), QColor(Qt::blue));

    // Redo: should be red again
    undoStack.redo();
    QCOMPARE(doc.frame(0).pixelColor(10, 10), QColor(Qt::red));
    QCOMPARE(doc.atlas().pixelColor(10, 10), QColor(Qt::red));
}

void TestPixelEditor::testEditSpritePixelsCommandMultiFrame()
{
    SpriteDocument doc;
    QImage atlas(64, 32, QImage::Format_ARGB32);
    atlas.fill(Qt::black);
    doc.setAtlas(atlas);

    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::white);
    SpriteBox b0(QRect(0, 0, 32, 32));
    doc.addFrame(f0, b0);

    QImage f1(32, 32, QImage::Format_ARGB32);
    f1.fill(Qt::white);
    SpriteBox b1(QRect(32, 0, 32, 32));
    doc.addFrame(f1, b1);

    QPainter pInit(&atlas);
    pInit.drawImage(0, 0, f0);
    pInit.drawImage(32, 0, f1);
    pInit.end();
    doc.setAtlas(atlas);

    QUndoStack undoStack;

    QMap<int, QImage> changes;
    QImage newF0 = f0.copy();
    newF0.fill(Qt::green);
    QImage newF1 = f1.copy();
    newF1.fill(Qt::yellow);

    changes[0] = newF0;
    changes[1] = newF1;

    undoStack.push(new EditSpritePixelsCommand(&doc, changes));

    QCOMPARE(doc.frame(0).pixelColor(5, 5), QColor(Qt::green));
    QCOMPARE(doc.frame(1).pixelColor(5, 5), QColor(Qt::yellow));
    QCOMPARE(doc.atlas().pixelColor(5, 5), QColor(Qt::green));
    QCOMPARE(doc.atlas().pixelColor(37, 5), QColor(Qt::yellow));

    undoStack.undo();
    QCOMPARE(doc.frame(0).pixelColor(5, 5), QColor(Qt::white));
    QCOMPARE(doc.frame(1).pixelColor(5, 5), QColor(Qt::white));
    QCOMPARE(doc.atlas().pixelColor(5, 5), QColor(Qt::white));
    QCOMPARE(doc.atlas().pixelColor(37, 5), QColor(Qt::white));
}

void TestPixelEditor::testEditSpritePixelsCommandUndoRedoAtlasIntegrity()
{
    SpriteDocument doc;
    QImage atlas(32, 32, QImage::Format_ARGB32);
    atlas.fill(Qt::darkGray);
    doc.setAtlas(atlas);

    QImage frame(16, 16, QImage::Format_ARGB32);
    frame.fill(Qt::cyan);
    SpriteBox box(QRect(8, 8, 16, 16));
    doc.addFrame(frame, box);

    // Initial atlas area check
    QCOMPARE(doc.atlas().pixelColor(8, 8), QColor(Qt::darkGray));

    // Paint frame transparent in center
    QImage edited = frame.copy();
    edited.setPixelColor(0, 0, Qt::transparent);

    QUndoStack undoStack;
    undoStack.push(new EditSpritePixelsCommand(&doc, 0, edited));

    // Pixel in atlas at (8, 8) must have alpha 0 due to CompositionMode_Source
    QCOMPARE(doc.atlas().pixelColor(8, 8).alpha(), 0);

    undoStack.undo();
    QCOMPARE(doc.atlas().pixelColor(8, 8), QColor(Qt::darkGray));
}

static void sendMouseEvent(QWidget *target, QEvent::Type type, const QPointF &pos,
                           Qt::MouseButton button = Qt::NoButton,
                           Qt::MouseButtons buttons = Qt::NoButton,
                           Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    Qt::MouseButtons bts = (buttons != Qt::NoButton) ? buttons : ((button != Qt::NoButton) ? Qt::MouseButtons(button) : Qt::NoButton);
    QMouseEvent ev(type, pos, pos, button, bts, modifiers);
    QApplication::sendEvent(target, &ev);
}

void TestPixelEditor::testPencilBresenhamHorizontal()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    canvas.setImage(img);

    canvas.setCurrentTool(PixelTool::Pencil);
    canvas.setPrimaryColor(Qt::red);

    // Draw horizontal line from (2, 4) to (7, 4)
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(2 * canvas.zoom(), 4 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseMove, QPointF(7 * canvas.zoom(), 4 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(7 * canvas.zoom(), 4 * canvas.zoom()), Qt::LeftButton);

    QImage result = canvas.image();
    for (int x = 2; x <= 7; ++x) {
        QCOMPARE(result.pixelColor(x, 4), QColor(Qt::red));
    }
    // Pixel (1, 4) and (8, 4) must remain transparent
    QCOMPARE(result.pixelColor(1, 4).alpha(), 0);
    QCOMPARE(result.pixelColor(8, 4).alpha(), 0);
}

void TestPixelEditor::testPencilBresenhamDiagonalContinuous()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    canvas.setImage(img);

    canvas.setCurrentTool(PixelTool::Pencil);
    canvas.setPrimaryColor(Qt::magenta);

    // Rapid jump from (1, 1) to (6, 10) (steep slope: dy > dx)
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(1 * canvas.zoom(), 1 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseMove, QPointF(6 * canvas.zoom(), 10 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(6 * canvas.zoom(), 10 * canvas.zoom()), Qt::LeftButton);

    QImage result = canvas.image();

    // Verify there are no row gaps between y = 1 and y = 10
    for (int y = 1; y <= 10; ++y) {
        bool rowHasPixel = false;
        for (int x = 0; x < 16; ++x) {
            if (result.pixelColor(x, y) == QColor(Qt::magenta)) {
                rowHasPixel = true;
                break;
            }
        }
        QVERIFY2(rowHasPixel, QString("Row %1 has no pixel in diagonal Bresenham stroke!").arg(y).toUtf8());
    }
}

void TestPixelEditor::testEraserClearsAlpha()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::white);
    canvas.setImage(img);

    canvas.setCurrentTool(PixelTool::Eraser);

    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(5 * canvas.zoom(), 5 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(5 * canvas.zoom(), 5 * canvas.zoom()), Qt::LeftButton);

    QImage result = canvas.image();
    QCOMPARE(result.pixelColor(5, 5).alpha(), 0);
    QCOMPARE(result.pixelColor(4, 5), QColor(Qt::white));
}

void TestPixelEditor::testBucketFillSolid()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::black);
    // Draw a 6x6 yellow box in the center
    for (int y = 5; y <= 10; ++y) {
        for (int x = 5; x <= 10; ++x) {
            img.setPixelColor(x, y, Qt::yellow);
        }
    }
    canvas.setImage(img);

    canvas.setCurrentTool(PixelTool::BucketFill);
    canvas.setPrimaryColor(Qt::cyan);

    // Click inside the yellow box
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(6 * canvas.zoom(), 6 * canvas.zoom()), Qt::LeftButton);

    QImage result = canvas.image();
    // Entire yellow box should now be cyan
    for (int y = 5; y <= 10; ++y) {
        for (int x = 5; x <= 10; ++x) {
            QCOMPARE(result.pixelColor(x, y), QColor(Qt::cyan));
        }
    }
    // Surrounding area should still be black
    QCOMPARE(result.pixelColor(0, 0), QColor(Qt::black));
}

void TestPixelEditor::testBucketFillBoundedBySelection()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::black);
    canvas.setImage(img);

    // Create a 4x4 rectangular selection at (2, 2) to (5, 5)
    canvas.setCurrentTool(PixelTool::SelectRect);
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(2 * canvas.zoom(), 2 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseMove, QPointF(5 * canvas.zoom(), 5 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(5 * canvas.zoom(), 5 * canvas.zoom()), Qt::LeftButton);

    QVERIFY(canvas.hasSelection());

    // Switch to bucket and fill at (3, 3) with red
    canvas.setCurrentTool(PixelTool::BucketFill);
    canvas.setPrimaryColor(Qt::red);

    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(3 * canvas.zoom(), 3 * canvas.zoom()), Qt::LeftButton);

    QImage result = canvas.image();
    // Selected area should be red
    for (int y = 2; y <= 5; ++y) {
        for (int x = 2; x <= 5; ++x) {
            QCOMPARE(result.pixelColor(x, y), QColor(Qt::red));
        }
    }
    // Pixels outside selection must remain black
    QCOMPARE(result.pixelColor(1, 1), QColor(Qt::black));
    QCOMPARE(result.pixelColor(6, 6), QColor(Qt::black));
}

void TestPixelEditor::testRectangularSelection()
{
    PixelCanvas canvas;
    QImage img(20, 20, QImage::Format_ARGB32);
    img.fill(Qt::white);
    canvas.setImage(img);

    canvas.selectAll();
    QVERIFY(canvas.hasSelection());
    QCOMPARE(canvas.selectionRect(), QRect(0, 0, 20, 20));

    canvas.deselect();
    QVERIFY(!canvas.hasSelection());
}

void TestPixelEditor::testColorSelectionMagicWand()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    // Draw 3 isolated red pixels
    img.setPixelColor(2, 3, Qt::red);
    img.setPixelColor(8, 8, Qt::red);
    img.setPixelColor(12, 14, Qt::red);
    canvas.setImage(img);

    canvas.setCurrentTool(PixelTool::SelectColor);

    // Click on pixel (2, 3) which is red
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(2 * canvas.zoom(), 3 * canvas.zoom()), Qt::LeftButton);

    QVERIFY(canvas.hasSelection());

    // Clear selection should erase all 3 red pixels to transparent
    canvas.clearSelection();
    QImage res = canvas.image();
    QCOMPARE(res.pixelColor(2, 3).alpha(), 0);
    QCOMPARE(res.pixelColor(8, 8).alpha(), 0);
    QCOMPARE(res.pixelColor(12, 14).alpha(), 0);
    // Surrounding blue pixels must remain blue
    QCOMPARE(res.pixelColor(0, 0), QColor(Qt::blue));
}

void TestPixelEditor::testClearSelection()
{
    PixelCanvas canvas;
    QImage img(10, 10, QImage::Format_ARGB32);
    img.fill(Qt::green);
    canvas.setImage(img);

    canvas.selectAll();
    canvas.clearSelection();

    QImage res = canvas.image();
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            QCOMPARE(res.pixelColor(x, y).alpha(), 0);
        }
    }
}

void TestPixelEditor::testCopyCutPasteFloating()
{
    PixelCanvas canvas;
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::black);
    // Draw a 2x2 white stamp at (1, 1)
    img.setPixelColor(1, 1, Qt::white);
    img.setPixelColor(2, 1, Qt::white);
    img.setPixelColor(1, 2, Qt::white);
    img.setPixelColor(2, 2, Qt::white);
    canvas.setImage(img);

    // Select this 2x2 stamp
    canvas.setCurrentTool(PixelTool::SelectRect);
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(1 * canvas.zoom(), 1 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseMove, QPointF(2 * canvas.zoom(), 2 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(2 * canvas.zoom(), 2 * canvas.zoom()), Qt::LeftButton);

    canvas.copySelection();

    // Paste clipboard
    canvas.pasteClipboard();
    canvas.commitFloatingSelection();

    QImage res = canvas.image();
    // Centered pasted stamp should exist
    int cx = (16 - 2) / 2;
    int cy = (16 - 2) / 2;
    QCOMPARE(res.pixelColor(cx, cy), QColor(Qt::white));
}

void TestPixelEditor::testFlipHorizontal()
{
    PixelCanvas canvas;
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    img.setPixelColor(0, 0, Qt::red);
    canvas.setImage(img);

    canvas.flipHorizontal();
    QImage res = canvas.image();
    QCOMPARE(res.pixelColor(3, 0), QColor(Qt::red));
    QCOMPARE(res.pixelColor(0, 0).alpha(), 0);
}

void TestPixelEditor::testFlipVertical()
{
    PixelCanvas canvas;
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    img.setPixelColor(0, 0, Qt::red);
    canvas.setImage(img);

    canvas.flipVertical();
    QImage res = canvas.image();
    QCOMPARE(res.pixelColor(0, 3), QColor(Qt::red));
    QCOMPARE(res.pixelColor(0, 0).alpha(), 0);
}

void TestPixelEditor::testRotate90CW()
{
    PixelCanvas canvas;
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    img.setPixelColor(0, 0, Qt::red);
    canvas.setImage(img);

    canvas.rotate90CW();
    QImage res = canvas.image();
    // After 90° clockwise rotation of 4x4 image, (0, 0) moves to (3, 0)
    QCOMPARE(res.pixelColor(3, 0), QColor(Qt::red));
}

void TestPixelEditor::testRetroPalettesAuthenticity()
{
    PixelEditorDialog dlg(nullptr, nullptr);

    // Verify ColorPalettePresets canonical sizes and single source of truth
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::Standard).size(), 36);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::NES).size(), 54);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::SNES).size(), 32);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::Amiga).size(), 32);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::PCEngine).size(), 32);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::GameBoyDMG).size(), 4);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::GameBoyPocket).size(), 4);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::Pico8).size(), 16);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::Commodore64).size(), 16);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::CGAMode1).size(), 4);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::CGAMode2).size(), 4);
    QCOMPARE(ColorPalettePresets::getPresetPalette(ColorPalettePresets::Endesga32).size(), 32);

    // Verify PixelEditorDialog forwards directly to ColorPalettePresets without duplicate tables
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Standard), ColorPalettePresets::getPresetPalette(ColorPalettePresets::Standard));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::NES), ColorPalettePresets::getPresetPalette(ColorPalettePresets::NES));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::SNES), ColorPalettePresets::getPresetPalette(ColorPalettePresets::SNES));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Amiga), ColorPalettePresets::getPresetPalette(ColorPalettePresets::Amiga));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::PCEngine), ColorPalettePresets::getPresetPalette(ColorPalettePresets::PCEngine));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::GameBoy), ColorPalettePresets::getPresetPalette(ColorPalettePresets::GameBoyDMG));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Pico8), ColorPalettePresets::getPresetPalette(ColorPalettePresets::Pico8));
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Commodore64), ColorPalettePresets::getPresetPalette(ColorPalettePresets::Commodore64));
}

void TestPixelEditor::testDynamicSpriteColorExtraction()
{
    SpriteDocument doc;
    QImage frame(8, 8, QImage::Format_ARGB32);
    frame.fill(Qt::transparent);
    frame.setPixelColor(0, 0, Qt::red);
    frame.setPixelColor(1, 0, Qt::red); // duplicate color
    frame.setPixelColor(2, 0, Qt::green);
    frame.setPixelColor(3, 0, Qt::blue);

    doc.addFrame(frame);

    PixelEditorDialog dlg(&doc, nullptr, 0);
    // Dialog should have extracted 3 unique opaque colors
    // We can verify through canvas image
    QImage img = dlg.findChild<PixelCanvas*>()->image();
    QSet<QRgb> colors;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            QRgb p = img.pixel(x, y);
            if (qAlpha(p) > 0) colors.insert(qRgb(qRed(p), qGreen(p), qBlue(p)));
        }
    }
    QCOMPARE(colors.size(), 3);
    QVERIFY(colors.contains(qRgb(255, 0, 0)));
    QVERIFY(colors.contains(qRgb(0, 255, 0)));
    QVERIFY(colors.contains(qRgb(0, 0, 255)));
}

void TestPixelEditor::testPixelCanvasDirtyRectUndoRedo()
{
    PixelCanvas canvas;
    QImage initialImg(64, 64, QImage::Format_ARGB32);
    initialImg.fill(Qt::white);
    canvas.setImage(initialImg);

    QCOMPARE(canvas.undoStack()->count(), 0);

    // 1. Simulate localized brush stroke at (10, 10) to (12, 12)
    QImage beforeStroke = canvas.image();
    QImage modified = beforeStroke.copy();
    for (int y = 10; y <= 12; ++y) {
        for (int x = 10; x <= 12; ++x) {
            modified.setPixelColor(x, y, Qt::red);
        }
    }
    // Set modified image directly on canvas to mimic finished stroke
    canvas.setImage(modified);
    canvas.undoStack()->clear(); // reset

    // Now test pushSnapshot with dirty rect
    canvas.pushSnapshot(beforeStroke, QStringLiteral("Pencil Red Dot"));
    QCOMPARE(canvas.undoStack()->count(), 1);

    // Verify current state is modified
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::red));
    QCOMPARE(canvas.image().pixelColor(12, 12), QColor(Qt::red));
    QCOMPARE(canvas.image().pixelColor(0, 0), QColor(Qt::white));
    QCOMPARE(canvas.image().pixelColor(50, 50), QColor(Qt::white));

    // 2. Undo: should restore only dirty rect to white without destroying the rest
    canvas.undo();
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::white));
    QCOMPARE(canvas.image().pixelColor(12, 12), QColor(Qt::white));
    QCOMPARE(canvas.image().pixelColor(0, 0), QColor(Qt::white));

    // 3. Redo: should re-apply dirty rect
    canvas.redo();
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::red));
    QCOMPARE(canvas.image().pixelColor(12, 12), QColor(Qt::red));
    QCOMPARE(canvas.image().pixelColor(0, 0), QColor(Qt::white));

    // 4. Test pushSnapshot with NO change (identical image)
    int countBefore = canvas.undoStack()->count();
    canvas.pushSnapshot(canvas.image(), QStringLiteral("No-op"));
    QCOMPARE(canvas.undoStack()->count(), countBefore); // Should not push command!
}

void TestPixelEditor::testOnionSkinTintedAndFalloff()
{
    // Create a 16x16 white square sprite
    QImage src(16, 16, QImage::Format_ARGB32);
    src.fill(Qt::white);

    // 1. Past frame (offset -1) at 50% opacity
    QImage pastImg1 = PixelCanvas::processOnionSkinLayer(src, -1, 50, OnionSkinEffect::TintedBlueRed, QSize(16, 16));
    QVERIFY(!pastImg1.isNull());
    QRgb p1 = pastImg1.pixel(8, 8);
    // Tinted blue: Blue component should dominate red
    QVERIFY(qBlue(p1) > qRed(p1));
    // Opacity: distance 1 has falloff 1.0 => alpha around 255 * 0.50 = 127
    QVERIFY(qAlpha(p1) >= 120 && qAlpha(p1) <= 135);

    // 2. Distant past frame (offset -3) at 50% opacity
    QImage pastImg3 = PixelCanvas::processOnionSkinLayer(src, -3, 50, OnionSkinEffect::TintedBlueRed, QSize(16, 16));
    QVERIFY(!pastImg3.isNull());
    QRgb p3 = pastImg3.pixel(8, 8);
    // Opacity with falloff 0.40 => alpha around 255 * 0.50 * 0.40 = 51
    QVERIFY(qAlpha(p3) < qAlpha(p1));
    QVERIFY(qAlpha(p3) >= 45 && qAlpha(p3) <= 58);

    // 3. Future frame (offset +1) at 50% opacity
    QImage futureImg1 = PixelCanvas::processOnionSkinLayer(src, 1, 50, OnionSkinEffect::TintedBlueRed, QSize(16, 16));
    QVERIFY(!futureImg1.isNull());
    QRgb f1 = futureImg1.pixel(8, 8);
    // Tinted red: Red component should dominate blue
    QVERIFY(qRed(f1) > qBlue(f1));
}

void TestPixelEditor::testOnionSkinEdgeDetection()
{
    // Create a 10x10 transparent image with an opaque 4x4 square in the center (from 3,3 to 6,6)
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(Qt::transparent);
    for (int y = 3; y <= 6; ++y) {
        for (int x = 3; x <= 6; ++x) {
            src.setPixelColor(x, y, Qt::white);
        }
    }

    QImage edgeImg = PixelCanvas::processOnionSkinLayer(src, -1, 100, OnionSkinEffect::EdgeDetection, QSize(10, 10));
    QVERIFY(!edgeImg.isNull());

    // (3,3) is an edge pixel (adjacent to transparent) => should be non-transparent
    QVERIFY(qAlpha(edgeImg.pixel(3, 3)) > 0);
    // (4,4) is an interior pixel (all 4 neighbors are opaque white) => should be transparent!
    QCOMPARE(qAlpha(edgeImg.pixel(4, 4)), 0);
    // (0,0) is outside the sprite => transparent
    QCOMPARE(qAlpha(edgeImg.pixel(0, 0)), 0);
}

void TestPixelEditor::testOnionSkinChannelsAndSilhouette()
{
    // Create an image with a specific color (R=200, G=150, B=100)
    QImage src(8, 8, QImage::Format_ARGB32);
    src.fill(qRgba(200, 150, 100, 255));

    // Channel R
    QImage rImg = PixelCanvas::processOnionSkinLayer(src, -1, 100, OnionSkinEffect::ChannelR, QSize(8, 8));
    QRgb rRgb = rImg.pixel(4, 4);
    QCOMPARE(qRed(rRgb), 200);
    QCOMPARE(qGreen(rRgb), 0);
    QCOMPARE(qBlue(rRgb), 0);

    // Channel G
    QImage gImg = PixelCanvas::processOnionSkinLayer(src, -1, 100, OnionSkinEffect::ChannelG, QSize(8, 8));
    QRgb gRgb = gImg.pixel(4, 4);
    QCOMPARE(qRed(gRgb), 0);
    QCOMPARE(qGreen(gRgb), 150);
    QCOMPARE(qBlue(gRgb), 0);

    // Channel B
    QImage bImg = PixelCanvas::processOnionSkinLayer(src, -1, 100, OnionSkinEffect::ChannelB, QSize(8, 8));
    QRgb bRgb = bImg.pixel(4, 4);
    QCOMPARE(qRed(bRgb), 0);
    QCOMPARE(qGreen(bRgb), 0);
    QCOMPARE(qBlue(bRgb), 100);

    // Silhouette
    QImage silImg = PixelCanvas::processOnionSkinLayer(src, -1, 100, OnionSkinEffect::Silhouette, QSize(8, 8));
    QRgb silRgb = silImg.pixel(4, 4);
    QCOMPARE(qRed(silRgb), 225);
    QCOMPARE(qGreen(silRgb), 230);
    QCOMPARE(qBlue(silRgb), 240);

    // TrueColor
    QImage tcImg = PixelCanvas::processOnionSkinLayer(src, -1, 80, OnionSkinEffect::TrueColor, QSize(8, 8));
    QRgb tcRgb = tcImg.pixel(4, 4);
    QCOMPARE(qRed(tcRgb), 200);
    QCOMPARE(qGreen(tcRgb), 150);
    QCOMPARE(qBlue(tcRgb), 100);
    QVERIFY(qAlpha(tcRgb) > 190 && qAlpha(tcRgb) <= 205);
}

void TestPixelEditor::testOnionSkinCanvasComposition()
{
    PixelCanvas canvas;
    QImage currentImg(16, 16, QImage::Format_ARGB32);
    currentImg.fill(Qt::transparent);
    canvas.setImage(currentImg);

    QImage prevImg(16, 16, QImage::Format_ARGB32);
    prevImg.fill(Qt::white);
    QImage nextImg(16, 16, QImage::Format_ARGB32);
    nextImg.fill(Qt::white);

    QVector<OnionSkinLayer> layers;
    layers.append({prevImg, -1});
    layers.append({nextImg, 1});

    canvas.setOnionSkinLayers(layers);
    canvas.setOnionSkinOpacity(60);
    canvas.setOnionSkinEffect(OnionSkinEffect::TintedBlueRed);
    canvas.setOnionSkinEnabled(true);

    QCOMPARE(canvas.onionSkinLayers().size(), 2);
    QCOMPARE(canvas.onionSkinOpacity(), 60);
    QCOMPARE(canvas.onionSkinEffect(), OnionSkinEffect::TintedBlueRed);
    QVERIFY(canvas.isOnionSkinEnabled());
}

void TestPixelEditor::testOnionSkinPivotAlignment()
{
    // Test that onion skin layer composite renders layer offset according to pivot alignment
    PixelCanvas canvas;
    QImage curImg(20, 20, QImage::Format_ARGB32);
    curImg.fill(Qt::transparent);
    canvas.setImage(curImg);

    // Create a 10x10 past layer image with a 2x2 white mark at (0, 0)
    QImage pastImg(10, 10, QImage::Format_ARGB32);
    pastImg.fill(Qt::transparent);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            pastImg.setPixelColor(x, y, QColor(255, 255, 255, 255));
        }
    }

    // Align offset = (5, 5) -> the 2x2 mark should now be at (5..6, 5..6) on the composite
    OnionSkinLayer layer;
    layer.image = pastImg;
    layer.relativeOffset = -1;
    layer.alignmentOffset = QPoint(5, 5);

    canvas.setOnionSkinLayers({layer});
    canvas.setOnionSkinOpacity(100);
    canvas.setOnionSkinEffect(OnionSkinEffect::TrueColor);
    canvas.setOnionSkinEnabled(true);

    const QImage &comp = canvas.onionSkinComposite();
    QVERIFY(!comp.isNull());
    QCOMPARE(comp.size(), QSize(20, 20));

    // Pixel at (0, 0) should be transparent
    QCOMPARE(qAlpha(comp.pixel(0, 0)), 0);

    // Pixel at (5, 5) should have the rendered mark with full alpha
    QRgb markRgb = comp.pixel(5, 5);
    QCOMPARE(qAlpha(markRgb), 255);
    QCOMPARE(qRed(markRgb), 255);
    QCOMPARE(qGreen(markRgb), 255);
    QCOMPARE(qBlue(markRgb), 255);
}

void TestPixelEditor::testOnionSkinPolygonClipping()
{
    SpriteDocument doc;
    QImage img1(16, 16, QImage::Format_ARGB32);
    img1.fill(Qt::white);
    QImage img2(16, 16, QImage::Format_ARGB32);
    img2.fill(Qt::white);

    SpriteBox box1(QRect(0, 0, 16, 16));
    SpriteBox box2(QRect(0, 0, 16, 16));

    // Polygon for box1: triangle (0,0), (8,0), (0,8)
    box1.polygon = QPolygonF({QPointF(0, 0), QPointF(8, 0), QPointF(0, 8)});
    box1.hasPolygonMesh = true;

    doc.setFrames({img1, img2}, {box1, box2});

    // Verify polygonClippedFrame on frame 0
    QImage clipped0 = doc.polygonClippedFrame(0);
    QVERIFY(!clipped0.isNull());
    // Point (1, 1) inside polygon should be opaque
    QVERIFY(qAlpha(clipped0.pixel(1, 1)) > 0);
    // Point (12, 12) outside triangle polygon should be transparent
    QCOMPARE(qAlpha(clipped0.pixel(12, 12)), 0);

    // Frame 1 has no polygon, so it should be unmodified
    QImage clipped1 = doc.polygonClippedFrame(1);
    QCOMPARE(qAlpha(clipped1.pixel(12, 12)), 255);
}

void TestPixelEditor::testSpaceAndMiddleMousePanning()
{
    PixelCanvas canvas;
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::white);
    canvas.setImage(img);

    int panDx = 0;
    int panDy = 0;
    QObject::connect(&canvas, &PixelCanvas::panRequested, [&panDx, &panDy](int dx, int dy) {
        panDx += dx;
        panDy += dy;
    });

    // 1. Press Space key
    QKeyEvent spacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &spacePress);

    // 2. Left click + drag with Space held
    QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(100, 100), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &pressEv);

    QMouseEvent moveEv(QEvent::MouseMove, QPointF(25, 30), QPointF(115, 120), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &moveEv);

    QCOMPARE(panDx, 15);
    QCOMPARE(panDy, 20);

    QMouseEvent releaseEv(QEvent::MouseButtonRelease, QPointF(25, 30), QPointF(115, 120), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &releaseEv);

    // 3. Release Space key
    QKeyEvent spaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &spaceRelease);
}

void TestPixelEditor::testAnimationScopedNavigation()
{
    SpriteDocument doc;
    QList<QImage> frames;
    QList<SpriteBox> boxes;
    for (int i = 0; i < 6; ++i) {
        QImage img(16, 16, QImage::Format_ARGB32);
        img.fill(qRgb(i * 40, 50, 60));
        frames.append(img);
        boxes.append(SpriteBox(QRect(0, 0, 16, 16)));
    }
    doc.setFrames(frames, boxes);

    // Create 2 animations:
    // "walk": frames [1, 3, 5]
    // "idle": frames [0, 2, 4]
    doc.addAnimation(QStringLiteral("walk"), {1, 3, 5});
    doc.addAnimation(QStringLiteral("idle"), {0, 2, 4});

    QUndoStack undoStack;

    // 1. Open editor starting on frame 3 (belongs to "walk")
    {
        PixelEditorDialog dlg(&doc, &undoStack, 3);
        QCOMPARE(dlg.currentFrameIndex(), 3);
        QCOMPARE(dlg.activeAnimationName(), QStringLiteral("walk"));
        QCOMPARE(dlg.activeSequence(), QList<int>({1, 3, 5}));

        // Combo should have "All Frames", "walk", "idle"
        QVERIFY(dlg.animationCombo() != nullptr);
        QCOMPARE(dlg.animationCombo()->count(), 3);

        // Next frame in "walk": should go to 5
        QKeyEvent pgDown(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier);
        QApplication::sendEvent(&dlg, &pgDown);
        QCOMPARE(dlg.currentFrameIndex(), 5);

        // Next again: at end of "walk", should stay at 5
        QApplication::sendEvent(&dlg, &pgDown);
        QCOMPARE(dlg.currentFrameIndex(), 5);

        // Previous frame in "walk": should go back to 3
        QKeyEvent pgUp(QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier);
        QApplication::sendEvent(&dlg, &pgUp);
        QCOMPARE(dlg.currentFrameIndex(), 3);

        // Previous again: should go to 1
        QApplication::sendEvent(&dlg, &pgUp);
        QCOMPARE(dlg.currentFrameIndex(), 1);

        // Previous again: at start of "walk", should stay at 1
        QApplication::sendEvent(&dlg, &pgUp);
        QCOMPARE(dlg.currentFrameIndex(), 1);

        // 2. Switch combo to "idle"
        int idleIdx = dlg.animationCombo()->findData(QStringLiteral("idle"));
        QVERIFY(idleIdx >= 0);
        dlg.animationCombo()->setCurrentIndex(idleIdx);
        // Current frame was 1 (not in idle), so switching jumps to first frame of idle: 0
        QCOMPARE(dlg.currentFrameIndex(), 0);
        QCOMPARE(dlg.activeAnimationName(), QStringLiteral("idle"));
        QCOMPARE(dlg.activeSequence(), QList<int>({0, 2, 4}));

        // Navigate forward in "idle"
        QApplication::sendEvent(&dlg, &pgDown);
        QCOMPARE(dlg.currentFrameIndex(), 2);
        QApplication::sendEvent(&dlg, &pgDown);
        QCOMPARE(dlg.currentFrameIndex(), 4);

        // 3. Switch combo to "All Frames" (index 0)
        dlg.animationCombo()->setCurrentIndex(0);
        QCOMPARE(dlg.activeAnimationName(), QString());
        QVERIFY(dlg.activeSequence().isEmpty());

        // Global navigation: from 4, previous goes to 3
        QApplication::sendEvent(&dlg, &pgUp);
        QCOMPARE(dlg.currentFrameIndex(), 3);
    }
}

void TestPixelEditor::testZoomPreservedAcrossFrameNavigation()
{
    SpriteDocument doc;
    QImage img0(32, 32, QImage::Format_ARGB32);
    img0.fill(Qt::red);
    QImage img1(24, 30, QImage::Format_ARGB32);
    img1.fill(Qt::green);
    QImage img2(40, 48, QImage::Format_ARGB32);
    img2.fill(Qt::blue);

    SpriteBox box0(QRect(0, 0, 32, 32));
    box0.pivot = QPoint(16, 32);
    box0.hasCustomPivot = true;

    SpriteBox box1(QRect(0, 0, 24, 30));
    box1.pivot = QPoint(12, 30);
    box1.hasCustomPivot = true;

    SpriteBox box2(QRect(0, 0, 40, 48));
    box2.pivot = QPoint(20, 48);
    box2.hasCustomPivot = true;

    doc.setFrames({img0, img1, img2}, {box0, box1, box2});

    PixelEditorDialog dlg(&doc, nullptr, 0);
    QVERIFY(dlg.canvas() != nullptr);

    // Set custom zoom to 22.0x
    dlg.canvas()->setZoom(22.0);
    QCOMPARE(dlg.canvas()->zoom(), 22.0);

    // Navigate to frame 1 (which has different dimensions 24x30)
    QKeyEvent pgDown(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier);
    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 1);

    // Zoom must be preserved at 22.0x (auto-zoom on frame switch disabled)
    QCOMPARE(dlg.canvas()->zoom(), 22.0);

    // Navigate to frame 2 (dimensions 40x48)
    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 2);
    QCOMPARE(dlg.canvas()->zoom(), 22.0);

    // Navigate back to frame 1
    QKeyEvent pgUp(QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier);
    QApplication::sendEvent(&dlg, &pgUp);
    QCOMPARE(dlg.currentFrameIndex(), 1);
    QCOMPARE(dlg.canvas()->zoom(), 22.0);
}

void TestPixelEditor::testPivotStrictlyStationaryAcrossAnimation()
{
    SpriteDocument doc;
    // 3 frames with different dimensions and different pivots
    QImage img0(32, 32, QImage::Format_ARGB32);
    img0.fill(Qt::red);
    QImage img1(24, 30, QImage::Format_ARGB32);
    img1.fill(Qt::green);
    QImage img2(40, 48, QImage::Format_ARGB32);
    img2.fill(Qt::blue);

    SpriteBox box0(QRect(0, 0, 32, 32));
    box0.pivot = QPoint(16, 32); // BottomCenter
    box0.hasCustomPivot = true;

    SpriteBox box1(QRect(0, 0, 24, 30));
    box1.pivot = QPoint(12, 30); // BottomCenter
    box1.hasCustomPivot = true;

    SpriteBox box2(QRect(0, 0, 40, 48));
    box2.pivot = QPoint(20, 48); // BottomCenter
    box2.hasCustomPivot = true;

    doc.setFrames({img0, img1, img2}, {box0, box1, box2});
    doc.addAnimation(QStringLiteral("run"), {0, 1, 2});

    PixelEditorDialog dlg(&doc, nullptr, 0);
    QVERIFY(dlg.canvas() != nullptr);

    // Initial frame 0
    QCOMPARE(dlg.currentFrameIndex(), 0);
    QPoint initialPivotPos = dlg.visualPivotPos();
    QVERIFY(!initialPivotPos.isNull());

    // Canvas size must be identical across the whole animation sequence
    QSize initialCanvasSize = dlg.canvas()->size();

    // Navigate to frame 1 (PageDown)
    QKeyEvent pgDown(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier);
    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 1);

    // Canvas widget size must remain identical
    QCOMPARE(dlg.canvas()->size(), initialCanvasSize);
    // Visual pivot coordinates in interface MUST BE STRICTLY IDENTICAL!
    QCOMPARE(dlg.visualPivotPos(), initialPivotPos);

    // Navigate to frame 2 (PageDown)
    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 2);
    QCOMPARE(dlg.canvas()->size(), initialCanvasSize);
    QCOMPARE(dlg.visualPivotPos(), initialPivotPos);

    // Navigate back to frame 0
    QKeyEvent pgUp(QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier);
    QApplication::sendEvent(&dlg, &pgUp);
    QApplication::sendEvent(&dlg, &pgUp);
    QCOMPARE(dlg.currentFrameIndex(), 0);
    QCOMPARE(dlg.visualPivotPos(), initialPivotPos);

    // Even if zoom changes, within that zoom level, navigating frames must keep the pivot strictly identical
    dlg.canvas()->setZoom(20.0);
    QPoint pivotAtZoom20 = dlg.visualPivotPos();
    QSize canvasSizeAtZoom20 = dlg.canvas()->size();

    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 1);
    QCOMPARE(dlg.canvas()->size(), canvasSizeAtZoom20);
    QCOMPARE(dlg.visualPivotPos(), pivotAtZoom20);

    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 2);
    QCOMPARE(dlg.canvas()->size(), canvasSizeAtZoom20);
    QCOMPARE(dlg.visualPivotPos(), pivotAtZoom20);
}

void TestPixelEditor::testPolygonRestrictionCheckboxAndEditing()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    // Frame 0: Has a diamond polygon: (16, 4) -> (28, 16) -> (16, 28) -> (4, 16)
    SpriteBox box0(QRect(0, 0, 32, 32));
    box0.hasPolygonMesh = true;
    box0.polygon = QPolygonF({QPointF(16, 4), QPointF(28, 16), QPointF(16, 28), QPointF(4, 16)});

    // Frame 1: No polygon mesh
    SpriteBox box1(QRect(0, 0, 32, 32));
    box1.hasPolygonMesh = false;
    box1.polygon = QPolygonF();

    doc.setFrames({img, img}, {box0, box1});

    PixelEditorDialog dlg(&doc, nullptr, 0);
    QVERIFY(dlg.canvas() != nullptr);
    QVERIFY(dlg.allowOutsidePolygonCheckBox() != nullptr);

    // 1. Frame 0 has a polygon: checkbox must be enabled and UNCHECKED by default
    QVERIFY(dlg.allowOutsidePolygonCheckBox()->isEnabled());
    QCOMPARE(dlg.allowOutsidePolygonCheckBox()->isChecked(), false);
    QCOMPARE(dlg.isEditingOutsidePolygonAllowed(), false);
    QCOMPARE(dlg.canvas()->allowEditingOutsidePolygon(), false);

    auto pixelToWidgetPt = [&](int px, int py) -> QPointF {
        double z = dlg.canvas()->zoom();
        QPoint off = dlg.canvas()->imageOffset();
        return QPointF((px + off.x() + 0.5) * z, (py + off.y() + 0.5) * z);
    };

    // 2. Try drawing with pencil outside polygon (at 2, 2, which is outside the diamond)
    dlg.canvas()->setCurrentTool(PixelTool::Pencil);
    dlg.canvas()->setPrimaryColor(Qt::red);

    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonPress, pixelToWidgetPt(2, 2), Qt::LeftButton);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonRelease, pixelToWidgetPt(2, 2), Qt::LeftButton);

    // Pixel at (2, 2) MUST REMAIN TRANSPARENT (editing outside has no effect!)
    QCOMPARE(dlg.canvas()->image().pixelColor(2, 2).alpha(), 0);

    // 3. Draw with pencil INSIDE polygon (at 16, 16, center of diamond)
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonPress, pixelToWidgetPt(16, 16), Qt::LeftButton);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonRelease, pixelToWidgetPt(16, 16), Qt::LeftButton);

    // Pixel at (16, 16) MUST BE RED
    QCOMPARE(dlg.canvas()->image().pixelColor(16, 16), QColor(Qt::red));

    // 4. Flood fill starting outside polygon (at 1, 1): should have no effect
    dlg.canvas()->setCurrentTool(PixelTool::BucketFill);
    dlg.canvas()->setPrimaryColor(Qt::blue);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonPress, pixelToWidgetPt(1, 1), Qt::LeftButton);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonRelease, pixelToWidgetPt(1, 1), Qt::LeftButton);

    // Pixel at (1, 1) still transparent
    QCOMPARE(dlg.canvas()->image().pixelColor(1, 1).alpha(), 0);

    // 5. Now CHECK the checkbox to allow editing outside polygon
    dlg.allowOutsidePolygonCheckBox()->setChecked(true);
    QCOMPARE(dlg.isEditingOutsidePolygonAllowed(), true);
    QCOMPARE(dlg.canvas()->allowEditingOutsidePolygon(), true);

    // Draw at (2, 2) again: now it MUST draw successfully!
    dlg.canvas()->setCurrentTool(PixelTool::Pencil);
    dlg.canvas()->setPrimaryColor(Qt::green);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonPress, pixelToWidgetPt(2, 2), Qt::LeftButton);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonRelease, pixelToWidgetPt(2, 2), Qt::LeftButton);

    QCOMPARE(dlg.canvas()->image().pixelColor(2, 2), QColor(Qt::green));

    // 6. Navigate to Frame 1 (no polygon): checkbox must be disabled
    QKeyEvent pgDown(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier);
    QApplication::sendEvent(&dlg, &pgDown);
    QCOMPARE(dlg.currentFrameIndex(), 1);
    QCOMPARE(dlg.allowOutsidePolygonCheckBox()->isEnabled(), false);
    QCOMPARE(dlg.isEditingOutsidePolygonAllowed(), true); // editing unrestricted
}

void TestPixelEditor::testPolygonFollowsFlipAndRotate()
{
    PixelCanvas canvas;
    QImage img(20, 30, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    // Initial polygon: triangle (0, 0), (10, 0), (0, 20)
    QPolygonF poly;
    poly << QPointF(0, 0) << QPointF(10, 0) << QPointF(0, 20);

    canvas.setImage(img);
    canvas.setPolygonMesh(poly);
    QVERIFY(canvas.hasPolygonMesh());
    QCOMPARE(canvas.polygonMesh(), poly);

    // 1. Flip Horizontal: width is 20 -> x' = 20 - x
    canvas.flipHorizontal();
    QPolygonF polyFlipH = canvas.polygonMesh();
    QCOMPARE(polyFlipH.size(), 3);
    QCOMPARE(polyFlipH[0], QPointF(20, 0));
    QCOMPARE(polyFlipH[1], QPointF(10, 0));
    QCOMPARE(polyFlipH[2], QPointF(20, 20));

    // Undo restore
    canvas.undo();
    QCOMPARE(canvas.polygonMesh(), poly);

    // 2. Flip Vertical: height is 30 -> y' = 30 - y
    canvas.flipVertical();
    QPolygonF polyFlipV = canvas.polygonMesh();
    QCOMPARE(polyFlipV.size(), 3);
    QCOMPARE(polyFlipV[0], QPointF(0, 30));
    QCOMPARE(polyFlipV[1], QPointF(10, 30));
    QCOMPARE(polyFlipV[2], QPointF(0, 10));

    // Undo restore
    canvas.undo();
    QCOMPARE(canvas.polygonMesh(), poly);

    // 3. Rotate 90° Clockwise: oldH is 30 -> (x', y') = (30 - y, x), new size 30x20
    canvas.rotate90CW();
    QPolygonF polyRot = canvas.polygonMesh();
    QCOMPARE(polyRot.size(), 3);
    QCOMPARE(polyRot[0], QPointF(30, 0));
    QCOMPARE(polyRot[1], QPointF(30, 10));
    QCOMPARE(polyRot[2], QPointF(10, 0));
    QCOMPARE(canvas.image().size(), QSize(30, 20));

    // Undo restore
    canvas.undo();
    QCOMPARE(canvas.polygonMesh(), poly);
    QCOMPARE(canvas.image().size(), QSize(20, 30));
}

void TestPixelEditor::testAtlasPolygonCollisionAndRepack()
{
    SpriteDocument doc;
    QImage atlas(100, 50, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);

    // Frame 0: 32x32 at atlas (0, 0)
    // Polygon in local coords: (0, 0), (20, 0), (0, 30) -> atlas coords [0..20, 0..30]
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::blue);
    SpriteBox b0(QRect(0, 0, 32, 32));
    b0.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(0, 30);
    b0.hasPolygonMesh = true;
    b0.vertices = b0.polygon.toList();
    b0.triangles = BentoPackGeometry::Triangulator::triangulate(b0.polygon);
    doc.addFrame(f0, b0);

    // Frame 1: 32x32 at atlas (15, 0)
    // Polygon in local coords: (10, 0), (30, 0), (30, 30) -> atlas coords [25..45, 0..30]
    // Initially: Frame 0 atlas [0..20], Frame 1 atlas [25..45] -> NO COLLISION
    QImage f1(32, 32, QImage::Format_ARGB32);
    f1.fill(Qt::red);
    SpriteBox b1(QRect(15, 0, 32, 32));
    b1.polygon << QPointF(10, 0) << QPointF(30, 0) << QPointF(30, 30);
    b1.hasPolygonMesh = true;
    b1.vertices = b1.polygon.toList();
    b1.triangles = BentoPackGeometry::Triangulator::triangulate(b1.polygon);
    doc.addFrame(f1, b1);

    QUndoStack undoStack;
    PixelEditorDialog dlg(&doc, &undoStack, 0);
    dlg.show();

    // Initially: No collision detected
    QCOMPARE(dlg.hasAtlasCollision(), false);
    QCOMPARE(dlg.collisionAlertLabel()->isVisible(), false);

    // Now flip Frame 0 horizontally: width is 32
    // Local polygon points become: (32, 0), (12, 0), (32, 30)
    // In atlas space (offset 0): x range is now [12..32]
    // Frame 1 in atlas space is [25..45]
    // OVERLAP between [25..32]!
    dlg.canvas()->flipHorizontal();

    // Collision should now be detected!
    QCOMPARE(dlg.hasAtlasCollision(), true);
    QCOMPARE(dlg.collisionAlertLabel()->isVisible(), true);
    QCOMPARE(dlg.repackButton()->isVisible(), true);
    QVERIFY(!dlg.collisionAlertLabel()->toolTip().isEmpty());

    // Execute repack via the pre-existing dialog (non-interactive mode for automated testing)
    bool repackSuccess = dlg.openAtlasPackingDialog(true);
    QVERIFY(repackSuccess);

    // After repack, the two frames are packed into non-colliding locations
    QCOMPARE(doc.boxes().size(), 2);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.box(0).hasPolygonMesh);
    QVERIFY(doc.box(1).hasPolygonMesh);

    // Verify after repack they no longer collide
    QPolygonF poly0Atlas = doc.box(0).polygon.translated(doc.box(0).rect.topLeft());
    QPolygonF poly1Atlas = doc.box(1).polygon.translated(doc.box(1).rect.topLeft());
    QPolygonF intersection = poly0Atlas.intersected(poly1Atlas);

    double isectArea = 0.0;
    for (int p = 0; p < intersection.size(); ++p) {
        const QPointF &p1 = intersection[p];
        const QPointF &p2 = intersection[(p + 1) % intersection.size()];
        isectArea += (p1.x() * p2.y() - p2.x() * p1.y());
    }
    isectArea = std::abs(isectArea) * 0.5;
    QVERIFY(isectArea < 0.1); // No overlap!
}

void TestPixelEditor::testApplyToAllFramesRelativePivot()
{
    SpriteDocument doc;
    QImage atlas(128, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);

    // Frame 0: 32x32, filled with blue, custom pivot at (16, 16)
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::blue);
    SpriteBox b0(QRect(0, 0, 32, 32));
    b0.hasCustomPivot = true;
    b0.pivot = QPoint(16, 16);
    doc.addFrame(f0, b0);

    // Frame 1: 48x48, filled with green, custom pivot at (24, 30)
    QImage f1(48, 48, QImage::Format_ARGB32);
    f1.fill(Qt::green);
    SpriteBox b1(QRect(32, 0, 48, 48));
    b1.hasCustomPivot = true;
    b1.pivot = QPoint(24, 30);
    // Add a triangle polygon mesh for frame 1: (0, 0), (48, 0), (24, 48)
    b1.hasPolygonMesh = true;
    b1.polygon << QPointF(0, 0) << QPointF(48, 0) << QPointF(24, 48);
    b1.vertices = b1.polygon.toList();
    b1.triangles = BentoPackGeometry::Triangulator::triangulate(b1.polygon);
    doc.addFrame(f1, b1);

    // Frame 2: 32x32, filled with yellow, custom pivot at (10, 10)
    QImage f2(32, 32, QImage::Format_ARGB32);
    f2.fill(Qt::yellow);
    SpriteBox b2(QRect(80, 0, 32, 32));
    b2.hasCustomPivot = true;
    b2.pivot = QPoint(10, 10);
    doc.addFrame(f2, b2);

    QUndoStack undoStack;
    PixelEditorDialog dlg(&doc, &undoStack, 0);

    // 1. Verify checkbox exists and is unchecked by default
    QVERIFY(dlg.applyToAllFramesCheckBox() != nullptr);
    QCOMPARE(dlg.isApplyToAllFramesEnabled(), false);
    QCOMPARE(dlg.applyToAllFramesCheckBox()->isChecked(), false);

    // 2. Modify Frame 0 with checkbox unchecked: only Frame 0 should be modified
    dlg.canvas()->setPrimaryColor(Qt::red);
    // Draw a pixel on Frame 0 at (18, 16)
    QImage oldImg0 = dlg.canvas()->image();
    QImage newImg0 = oldImg0;
    newImg0.setPixelColor(18, 16, Qt::red);
    dlg.canvas()->setImage(newImg0);
    dlg.canvas()->pushSnapshot(oldImg0, QStringLiteral("Pencil"), QPolygonF(), CanvasAction::Pencil);

    // Frame 1 in sessionModifiedFrames should not exist yet
    QVERIFY(!dlg.sessionModifiedFrames().contains(1));

    // 3. Now check "Apply to all frames"
    dlg.setApplyToAllFrames(true);
    QCOMPARE(dlg.isApplyToAllFramesEnabled(), true);

    // Draw on Frame 0 at (16 + 2, 16 + 5) = (18, 21) in magenta
    // Offset relative to Frame 0 pivot (16, 16) is dx = +2, dy = +5
    oldImg0 = dlg.canvas()->image();
    newImg0 = oldImg0;
    newImg0.setPixelColor(18, 21, Qt::magenta);
    dlg.canvas()->setImage(newImg0);
    dlg.canvas()->pushSnapshot(oldImg0, QStringLiteral("Pencil"), QPolygonF(), CanvasAction::Pencil);

    // Verify Frame 0 has magenta at (18, 21)
    QCOMPARE(dlg.canvas()->image().pixelColor(18, 21), QColor(Qt::magenta));

    // Verify Frame 1 received the magenta pixel relative to its pivot (24, 30)!
    // Target position: (24 + 2, 30 + 5) = (26, 35)
    QVERIFY(dlg.sessionModifiedFrames().contains(1));
    QImage modifiedF1 = dlg.sessionModifiedFrames().value(1);
    QCOMPARE(modifiedF1.pixelColor(26, 35), QColor(Qt::magenta));
    // Verify other pixels in Frame 1 remain green
    QCOMPARE(modifiedF1.pixelColor(0, 0), QColor(Qt::green));

    // Verify Frame 2 received the magenta pixel relative to its pivot (10, 10)!
    // Target position: (10 + 2, 10 + 5) = (12, 15)
    QVERIFY(dlg.sessionModifiedFrames().contains(2));
    QImage modifiedF2 = dlg.sessionModifiedFrames().value(2);
    QCOMPARE(modifiedF2.pixelColor(12, 15), QColor(Qt::magenta));
    QCOMPARE(modifiedF2.pixelColor(0, 0), QColor(Qt::yellow));

    // 4. Test Multi-frame Undo and Redo
    dlg.canvas()->undo();
    // Frame 0 magenta pixel should be reverted (back to blue)
    QCOMPARE(dlg.canvas()->image().pixelColor(18, 21), QColor(Qt::blue));
    // Frame 1 and Frame 2 should also be reverted
    QCOMPARE(dlg.sessionModifiedFrames().value(1).pixelColor(26, 35), QColor(Qt::green));
    QCOMPARE(dlg.sessionModifiedFrames().value(2).pixelColor(12, 15), QColor(Qt::yellow));

    // Redo
    dlg.canvas()->redo();
    QCOMPARE(dlg.canvas()->image().pixelColor(18, 21), QColor(Qt::magenta));
    QCOMPARE(dlg.sessionModifiedFrames().value(1).pixelColor(26, 35), QColor(Qt::magenta));
    QCOMPARE(dlg.sessionModifiedFrames().value(2).pixelColor(12, 15), QColor(Qt::magenta));

    // 5. Test Flip Horizontal across all frames
    dlg.canvas()->flipHorizontal();
    // Frame 0 is 32 wide: (18, 21) flipped becomes (32 - 1 - 18, 21) = (13, 21)
    QCOMPARE(dlg.canvas()->image().pixelColor(13, 21), QColor(Qt::magenta));

    // Frame 1 is 48 wide: (26, 35) flipped becomes (48 - 1 - 26, 35) = (21, 35)
    QImage flippedF1 = dlg.sessionModifiedFrames().value(1);
    QCOMPARE(flippedF1.pixelColor(21, 35), QColor(Qt::magenta));
    // And polygon of Frame 1 is flipped: w - pt.x()
    QPolygonF poly1Flipped = dlg.sessionModifiedPolygons().value(1);
    QCOMPARE(poly1Flipped.size(), 3);
    QCOMPARE(poly1Flipped[0], QPointF(48 - 0, 0));
    QCOMPARE(poly1Flipped[1], QPointF(48 - 48, 0));
    QCOMPARE(poly1Flipped[2], QPointF(48 - 24, 48));

    // 6. Test Flood Fill across all frames
    // 6. Test Flood Fill across all frames (command-level execution, not pixel-diff copy!)
    // On Frame 0: draw a small 5x5 box of red with a black border around pivot (16, 16)
    // and flood-fill inside at (16, 16) with Qt::white.
    // Frame 0 will only change inside the small 5x5 box.
    // Frame 1 is 48x48 filled with green inside a large triangle polygon.
    // Because flood-fill executes at the command level relative to target pivot (24, 30),
    // Frame 1 floods its local green color across its entire connected polygon region!
    {
        QImage f0Img = dlg.canvas()->image();
        // Draw black border around [14..18] x [14..18]
        for (int i = 14; i <= 18; ++i) {
            f0Img.setPixelColor(i, 14, Qt::black);
            f0Img.setPixelColor(i, 18, Qt::black);
            f0Img.setPixelColor(14, i, Qt::black);
            f0Img.setPixelColor(18, i, Qt::black);
        }
        // Fill inside [15..17] x [15..17] with red
        for (int y = 15; y <= 17; ++y) {
            for (int x = 15; x <= 17; ++x) {
                f0Img.setPixelColor(x, y, Qt::red);
            }
        }
        dlg.canvas()->setImage(f0Img);
    }

    // Now invoke bucket flood fill at pivot (16, 16) with white
    dlg.canvas()->applyFloodFill(16, 16, Qt::white);

    // Frame 0: (16, 16) is white, but pixels outside the black border (e.g. at (16, 10)) are NOT white
    QCOMPARE(dlg.canvas()->image().pixelColor(16, 16), QColor(Qt::white));
    QVERIFY(dlg.canvas()->image().pixelColor(16, 10) != QColor(Qt::white));

    // Frame 1: target pivot is (24, 30). Local color was green.
    // Command-level flood fill must flood Frame 1's connected green pixels across its polygon!
    QVERIFY(dlg.sessionModifiedFrames().contains(1));
    QImage floodedF1 = dlg.sessionModifiedFrames().value(1);
    // (24, 30) is white
    QCOMPARE(floodedF1.pixelColor(24, 30), QColor(Qt::white));
    // Pixel (24, 10) on Frame 1 is inside the polygon and was green:
    // It MUST be flooded white, proving Frame 1's flooding depended on Frame 1's local pixels and shape!
    QCOMPARE(floodedF1.pixelColor(24, 10), QColor(Qt::white));

    // 6.5. Test Multi-frame Undo/Redo across frame navigation
    // Navigate to Frame 1: undo stack must NOT be cleared!
    dlg.onNextFrame();
    QCOMPARE(dlg.currentFrameIndex(), 1);
    QCOMPARE(dlg.canvas()->image().pixelColor(24, 30), QColor(Qt::white));
    QVERIFY(dlg.canvas()->undoStack() != nullptr);
    QVERIFY(dlg.canvas()->undoStack()->canUndo());

    // Undo from Frame 1
    dlg.canvas()->undo();
    // Frame 1 on-canvas should be reverted immediately
    QVERIFY(dlg.canvas()->image().pixelColor(24, 30) != QColor(Qt::white));
    // Frame 0 in session memory should also be reverted
    QVERIFY(dlg.sessionModifiedFrames().value(0).pixelColor(16, 16) != QColor(Qt::white));

    // Navigate to Frame 0 to verify on-canvas
    dlg.onPreviousFrame();
    QCOMPARE(dlg.currentFrameIndex(), 0);
    QVERIFY(dlg.canvas()->image().pixelColor(16, 16) != QColor(Qt::white));

    // Redo from Frame 0
    dlg.canvas()->redo();
    QCOMPARE(dlg.canvas()->image().pixelColor(16, 16), QColor(Qt::white));
    QCOMPARE(dlg.sessionModifiedFrames().value(1).pixelColor(24, 30), QColor(Qt::white));

    // Undo again to leave clean
    dlg.canvas()->undo();

    // 7. Test Animation Scope Filter
    doc.addAnimation(QStringLiteral("walk"), {0, 1}); // Frame 2 is NOT part of this animation

    // Re-create dialog on frame 0 to populate animation combo
    PixelEditorDialog dlgAnim(&doc, &undoStack, 0);
    dlgAnim.setApplyToAllFrames(true);

    // Select the "walk" animation in combo
    int walkIdx = dlgAnim.animationCombo()->findData(QStringLiteral("walk"));
    QVERIFY(walkIdx >= 0);
    dlgAnim.animationCombo()->setCurrentIndex(walkIdx);
    QCOMPARE(dlgAnim.activeSequence(), QList<int>({0, 1}));

    // Draw on Frame 0 with animation filter active
    oldImg0 = dlgAnim.canvas()->image();
    newImg0 = oldImg0;
    newImg0.setPixelColor(16, 16, Qt::cyan);
    dlgAnim.canvas()->setImage(newImg0);
    dlgAnim.canvas()->pushSnapshot(oldImg0, QStringLiteral("Pencil"), QPolygonF(), CanvasAction::Pencil);

    // Frame 1 is in "walk" -> must be modified
    QVERIFY(dlgAnim.sessionModifiedFrames().contains(1));
    QCOMPARE(dlgAnim.sessionModifiedFrames().value(1).pixelColor(24, 30), QColor(Qt::cyan));

    // Frame 2 is NOT in "walk" -> must NOT be modified
    QVERIFY(!dlgAnim.sessionModifiedFrames().contains(2));
}

void TestPixelEditor::testProColorPickerAndHarmonies()
{
    // 1. Color Wheel & Harmony calculation
    ColorWheelWidget wheel;
    QColor baseRed = QColor::fromHsv(0, 200, 220); // Pure red with saturation and brightness
    wheel.setColor(baseRed);
    QCOMPARE(wheel.color(), baseRed);

    // Complementary: opposite hue (+180° = Cyan)
    wheel.setHarmonyRule(ColorHarmonyRule::Complementary);
    auto comp = wheel.currentHarmonies();
    QCOMPARE(comp.size(), 1);
    QCOMPARE(comp[0].hsvHue(), 180);

    // Triadic: +120° and +240° (Green and Blue)
    wheel.setHarmonyRule(ColorHarmonyRule::Triadic);
    auto tri = wheel.currentHarmonies();
    QCOMPARE(tri.size(), 2);
    QCOMPARE(tri[0].hsvHue(), 120);
    QCOMPARE(tri[1].hsvHue(), 240);

    // Tetradic: +90°, +180°, +270°
    wheel.setHarmonyRule(ColorHarmonyRule::Tetradic);
    auto tetra = wheel.currentHarmonies();
    QCOMPARE(tetra.size(), 3);
    QCOMPARE(tetra[0].hsvHue(), 90);
    QCOMPARE(tetra[1].hsvHue(), 180);
    QCOMPARE(tetra[2].hsvHue(), 270);

    // Analogous: -30° and +30°
    wheel.setHarmonyRule(ColorHarmonyRule::Analogous);
    auto ana = wheel.currentHarmonies();
    QCOMPARE(ana.size(), 2);
    QCOMPARE(ana[0].hsvHue(), 330);
    QCOMPARE(ana[1].hsvHue(), 30);

    // 2. 2D Map (Saturation-Value box + Hue strip)
    ColorMap2DWidget map2D;
    map2D.setColor(QColor(100, 150, 200));
    QCOMPARE(map2D.color(), QColor(100, 150, 200));

    // 3. RGB & HSV Sliders
    ColorSlidersWidget sliders;
    sliders.setColor(QColor(173, 93, 55)); // #ad5d37 from Aseprite screenshot
    QCOMPARE(sliders.color().red(), 173);
    QCOMPARE(sliders.color().green(), 93);
    QCOMPARE(sliders.color().blue(), 55);

    // 4. Integrated ColorPickerWidget
    ColorPickerWidget picker;
    picker.setColor(QColor(50, 120, 240));
    QCOMPARE(picker.color(), QColor(50, 120, 240));

    // Switching view modes
    picker.setViewMode(ColorPickerWidget::WheelMode);
    QCOMPARE(picker.viewMode(), ColorPickerWidget::WheelMode);
    picker.setViewMode(ColorPickerWidget::Map2DMode);
    QCOMPARE(picker.viewMode(), ColorPickerWidget::Map2DMode);
    picker.setViewMode(ColorPickerWidget::SlidersMode);
    QCOMPARE(picker.viewMode(), ColorPickerWidget::SlidersMode);

    // Test ProColorPickerDialog instantiation
    ProColorPickerDialog dlg(QColor(255, 128, 0));
    QCOMPARE(dlg.selectedColor(), QColor(255, 128, 0));
}

void TestPixelEditor::testPixelEditorNonAtlasFiltersAndAnimationScope()
{
    // Setup document with 3 frames
    SpriteDocument doc;
    QImage f0(16, 16, QImage::Format_ARGB32);
    f0.fill(qRgba(255, 0, 0, 255)); // Red
    QImage f1(16, 16, QImage::Format_ARGB32);
    f1.fill(qRgba(0, 255, 0, 255)); // Green
    QImage f2(16, 16, QImage::Format_ARGB32);
    f2.fill(qRgba(0, 0, 255, 255)); // Blue

    doc.setFrames({f0, f1, f2}, {SpriteBox(QRect(0, 0, 16, 16)), SpriteBox(QRect(16, 0, 16, 16)), SpriteBox(QRect(32, 0, 16, 16))});

    doc.addAnimation(QStringLiteral("walk"), {0, 1});

    QUndoStack docUndoStack;
    PixelEditorDialog dialog(&doc, &docUndoStack, 0);

    // 1. Verify filtersButton exists and is populated
    QToolButton *filtersBtn = dialog.filtersButton();
    QVERIFY(filtersBtn != nullptr);
    QVERIFY(filtersBtn->menu() != nullptr);

    QList<QAction*> actions = filtersBtn->menu()->actions();
    QStringList actionTexts;
    for (QAction *act : actions) {
        if (!act->isSeparator() && !act->isIconVisibleInMenu() && !act->text().isEmpty()) {
            actionTexts.append(act->text());
        }
    }

    // Verify non-atlas filters are present
    FilterRegistry &reg = FilterRegistry::instance();
    for (FilterPlugin *f : reg.filters()) {
        if (f->isAtlasModifier()) {
            // Must NOT be present
            for (const QString &txt : actionTexts) {
                QVERIFY(!txt.contains(f->name()));
            }
        }
    }

    // Verify at least one non-atlas filter action exists
    QVERIFY(!actions.isEmpty());

    // 2. Verify checkbox text dynamically updates with animation
    QCheckBox *checkAll = dialog.applyToAllFramesCheckBox();
    QVERIFY(checkAll != nullptr);
    QVERIFY(!checkAll->isChecked());

    // Switch to "walk" animation
    QComboBox *animCombo = dialog.animationCombo();
    QVERIFY(animCombo != nullptr);
    int walkIdx = animCombo->findText(QStringLiteral("walk (2 frames)"));
    if (walkIdx < 0) {
        walkIdx = animCombo->findData(QStringLiteral("walk"));
    }
    if (walkIdx >= 0) {
        animCombo->setCurrentIndex(walkIdx);
    }

    QVERIFY(checkAll->text().contains(QStringLiteral("walk")) || checkAll->toolTip().contains(QStringLiteral("walk")));

    // 3. Test filter application with custom simulated filter
    class TestMockFilter : public FilterPlugin {
    public:
        QString id() const override { return QStringLiteral("test_mock_filter"); }
        QString name() const override { return QStringLiteral("Mock Inverter"); }
        QString description() const override { return QStringLiteral("Test mock filter"); }
        QString category() const override { return QStringLiteral("Colors & Palettes"); }
        FilterModifierFlags modifierFlags() const override { return PixelModifier; }

        FilterDialogBase* createDialog(SpriteDocument *d, QUndoStack*, QWidget *parent) override {
            class MockDlg : public FilterDialogBase {
            public:
                MockDlg(SpriteDocument *doc, QWidget *p) : FilterDialogBase(doc, nullptr, p) {
                    QTimer::singleShot(0, this, &QDialog::accept);
                }
                void applyPreview() override {
                    if (!m_document) return;
                    QImage atlas = m_document->atlas();
                    atlas.invertPixels(QImage::InvertRgb);
                    m_document->setAtlas(atlas);
                    QList<QImage> fList;
                    for (const auto &b : m_document->boxes()) {
                        fList.append(atlas.copy(b.rect));
                    }
                    m_document->setFrames(fList, m_document->boxes());
                }
                QUndoCommand* createUndoCommand() override { return nullptr; }
                void resetDefaults() override {}
            };
            return new MockDlg(d, parent);
        }
    };

    TestMockFilter mockFilter;
    QVERIFY(!mockFilter.isAtlasModifier());
    QVERIFY(mockFilter.isPixelModifier());

    // A. Single frame mode (checkbox off)
    dialog.setApplyToAllFrames(false);
    dialog.loadFrame(0);
    dialog.applyFilterToSession(&mockFilter);

    // Frame 0 should be modified and inverted
    QVERIFY(dialog.sessionModifiedFrames().contains(0));
    QColor invertedPixel0 = dialog.canvas()->image().pixelColor(0, 0);
    QCOMPARE(invertedPixel0.red(), 0);
    QCOMPARE(invertedPixel0.green(), 255);
    QCOMPARE(invertedPixel0.blue(), 255);

    // Frame 1 should NOT be modified
    QVERIFY(!dialog.sessionModifiedFrames().contains(1));

    // Test Undo
    dialog.canvas()->undo();
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(255, 0, 0, 255));

    // B. Multi-frame animation mode (checkbox ON)
    dialog.setApplyToAllFrames(true);
    dialog.applyFilterToSession(&mockFilter);

    // Both frames 0 and 1 of "walk" should be modified
    QVERIFY(dialog.sessionModifiedFrames().contains(0));
    QVERIFY(dialog.sessionModifiedFrames().contains(1));
    // Frame 2 is NOT in "walk", so it must remain untouched
    QVERIFY(!dialog.sessionModifiedFrames().contains(2));

    // Frame 1 inverted (was green -> now magenta 255, 0, 255)
    QColor invertedPixel1 = dialog.sessionModifiedFrames()[1].pixelColor(0, 0);
    QCOMPARE(invertedPixel1.red(), 255);
    QCOMPARE(invertedPixel1.green(), 0);
    QCOMPARE(invertedPixel1.blue(), 255);

    // Undo multi-frame filter
    dialog.canvas()->undo();
    dialog.loadFrame(0);
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(255, 0, 0, 255));
}

void TestPixelEditor::testPixelEditorFilterLivePreviewAndRollback()
{
    // Prepare document with a single 16x16 solid red frame
    QImage redImg(16, 16, QImage::Format_ARGB32);
    redImg.fill(QColor(255, 0, 0, 255));

    SpriteDocument doc;
    doc.setAtlas(redImg);
    SpriteBox box(QRect(0, 0, 16, 16));
    box.index = 0;
    doc.setFrames({redImg}, {box});

    PixelEditorDialog dialog(&doc, nullptr, 0);
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(255, 0, 0, 255));

    // Custom Mock Filter that enables interactive live preview validation
    class LivePreviewTestFilter : public FilterPlugin {
    public:
        enum ActionToPerform { TestPreviewThenReject, TestPreviewThenAccept };
        ActionToPerform action = TestPreviewThenReject;
        PixelCanvas *observedCanvas = nullptr;
        bool previewSeenOnCanvas = false;
        bool restoreSeenOnCanvas = false;

        QString id() const override { return QStringLiteral("live_preview_filter"); }
        QString name() const override { return QStringLiteral("Live Preview Filter"); }
        QString description() const override { return QStringLiteral("Test live preview filter"); }
        QString category() const override { return QStringLiteral("Effects"); }
        FilterModifierFlags modifierFlags() const override { return PixelModifier; }

        FilterDialogBase* createDialog(SpriteDocument *d, QUndoStack*, QWidget *parent) override {
            class LiveDlg : public FilterDialogBase {
            public:
                LivePreviewTestFilter *m_parentFilter = nullptr;
                LiveDlg(SpriteDocument *doc, LivePreviewTestFilter *pf, QWidget *p)
                    : FilterDialogBase(doc, nullptr, p), m_parentFilter(pf)
                {
                    QTimer::singleShot(10, this, [this]() {
                        // 1. Apply preview: fill tempDoc with blue
                        applyPreview();
                        // Verify that observed canvas immediately received the preview!
                        if (m_parentFilter && m_parentFilter->observedCanvas) {
                            QColor col = m_parentFilter->observedCanvas->image().pixelColor(0, 0);
                            if (col == QColor(0, 0, 255, 255)) {
                                m_parentFilter->previewSeenOnCanvas = true;
                            }
                        }

                        // 2. Simulate user toggling Live Preview checkbox OFF
                        restoreInitialState();
                        // Verify that observed canvas reverted back to original red!
                        if (m_parentFilter && m_parentFilter->observedCanvas) {
                            QColor col = m_parentFilter->observedCanvas->image().pixelColor(0, 0);
                            if (col == QColor(255, 0, 0, 255)) {
                                m_parentFilter->restoreSeenOnCanvas = true;
                            }
                        }

                        // 3. Re-apply preview
                        applyPreview();

                        // 4. Either reject or accept based on test configuration
                        if (m_parentFilter && m_parentFilter->action == TestPreviewThenReject) {
                            reject();
                        } else {
                            accept();
                        }
                    });
                }

                void applyPreview() override {
                    if (!m_document) return;
                    QImage blueAtlas(16, 16, QImage::Format_ARGB32);
                    blueAtlas.fill(QColor(0, 0, 255, 255));
                    m_document->setAtlas(blueAtlas);
                    m_document->setFrames({blueAtlas}, m_document->boxes());
                }

                QUndoCommand* createUndoCommand() override { return nullptr; }
                void resetDefaults() override {}
            };
            return new LiveDlg(d, this, parent);
        }
    };

    LivePreviewTestFilter testFilter;
    testFilter.observedCanvas = dialog.canvas();

    // --- Scenario 1: Live preview followed by Cancel (Reject) ---
    testFilter.action = LivePreviewTestFilter::TestPreviewThenReject;
    testFilter.previewSeenOnCanvas = false;
    testFilter.restoreSeenOnCanvas = false;

    dialog.applyFilterToSession(&testFilter);

    // Canvas must have experienced the real-time preview and restore
    QVERIFY(testFilter.previewSeenOnCanvas);
    QVERIFY(testFilter.restoreSeenOnCanvas);

    // Because user cancelled (rejected), canvas must be pristine red and session unmodified
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(255, 0, 0, 255));
    QVERIFY(!dialog.sessionModifiedFrames().contains(0));

    // --- Scenario 2: Live preview followed by OK (Accept) ---
    testFilter.action = LivePreviewTestFilter::TestPreviewThenAccept;
    testFilter.previewSeenOnCanvas = false;
    testFilter.restoreSeenOnCanvas = false;

    dialog.applyFilterToSession(&testFilter);

    // Filter changes must be applied and committed
    QVERIFY(testFilter.previewSeenOnCanvas);
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(0, 0, 255, 255));
    QVERIFY(dialog.sessionModifiedFrames().contains(0));

    // Undo must restore the original red image
    dialog.canvas()->undo();
    QCOMPARE(dialog.canvas()->image().pixelColor(0, 0), QColor(255, 0, 0, 255));
}


// ============================================================================
// 9. Multi-Layer Editing & Layer Stack Tests (M18)
// ============================================================================

void TestPixelEditor::testLayerStackWidgetAndControls()
{
    LayerStackWidget widget;
    CanvasLayer l1;
    l1.id = QStringLiteral("bg");
    l1.name = QStringLiteral("Background");
    l1.visible = true;
    l1.locked = false;
    l1.opacity = 255;
    l1.zOrder = 0;
    l1.image = QImage(32, 32, QImage::Format_ARGB32);
    l1.image.fill(Qt::white);

    CanvasLayer l2;
    l2.id = QStringLiteral("fg");
    l2.name = QStringLiteral("Lineart");
    l2.visible = true;
    l2.locked = false;
    l2.opacity = 200;
    l2.zOrder = 10;
    l2.image = QImage(32, 32, QImage::Format_ARGB32);
    l2.image.fill(Qt::transparent);

    widget.setLayers({l1, l2}, 1);
    QCOMPARE(widget.activeIndex(), 1);

    int activeSignalIdx = -1;
    connect(&widget, &LayerStackWidget::activeLayerChanged, [&activeSignalIdx](int idx) {
        activeSignalIdx = idx;
    });

    widget.setActiveIndex(0);
    QCOMPARE(activeSignalIdx, 0);
    QCOMPARE(widget.activeIndex(), 0);

    // Verify toolbar button signals
    bool addReq = false;
    connect(&widget, &LayerStackWidget::addLayerRequested, [&addReq]() { addReq = true; });
    widget.addLayerButton()->click();
    QVERIFY(addReq);

    bool dupReq = false;
    connect(&widget, &LayerStackWidget::duplicateLayerRequested, [&dupReq]() { dupReq = true; });
    widget.duplicateLayerButton()->click();
    QVERIFY(dupReq);

    bool delReq = false;
    connect(&widget, &LayerStackWidget::removeLayerRequested, [&delReq]() { delReq = true; });
    widget.removeLayerButton()->click();
    QVERIFY(delReq);

    bool upReq = false;
    connect(&widget, &LayerStackWidget::moveLayerUpRequested, [&upReq]() { upReq = true; });
    widget.moveLayerUpButton()->click();
    QVERIFY(upReq);

    widget.setActiveIndex(1);

    bool downReq = false;
    connect(&widget, &LayerStackWidget::moveLayerDownRequested, [&downReq]() { downReq = true; });
    widget.moveLayerDownButton()->click();
    QVERIFY(downReq);

    bool mergeReq = false;
    connect(&widget, &LayerStackWidget::mergeLayerDownRequested, [&mergeReq]() { mergeReq = true; });
    widget.mergeLayerDownButton()->click();
    QVERIFY(mergeReq);

    bool flatReq = false;
    connect(&widget, &LayerStackWidget::flattenLayersRequested, [&flatReq]() { flatReq = true; });
    widget.flattenLayersButton()->click();
    QVERIFY(flatReq);

    // Verify options checkboxes
    bool sampleChanged = false;
    connect(&widget, &LayerStackWidget::sampleAllLayersChanged, [&sampleChanged](bool b) { sampleChanged = b; });
    widget.sampleAllLayersCheckBox()->setChecked(true);
    QVERIFY(sampleChanged);
    QVERIFY(widget.isSampleAllLayers());

    bool onionRestricted = false;
    connect(&widget, &LayerStackWidget::onionSkinCurrentLayerOnlyChanged, [&onionRestricted](bool b) { onionRestricted = b; });
    widget.onionSkinCurrentLayerOnlyCheckBox()->setChecked(true);
    QVERIFY(onionRestricted);
    QVERIFY(widget.isOnionSkinCurrentLayerOnly());

    // Verify opacity slider
    quint8 newOp = 0;
    connect(&widget, &LayerStackWidget::layerOpacityChanged, [&newOp](int, quint8 op) { newOp = op; });
    widget.opacitySlider()->setValue(50);
    QCOMPARE(newOp, 128);
}

void TestPixelEditor::testMultiLayerCanvasDrawingAndLock()
{
    PixelCanvas canvas;
    canvas.setZoom(1.0);

    CanvasLayer bg;
    bg.id = QStringLiteral("bg");
    bg.name = QStringLiteral("Background");
    bg.visible = true;
    bg.locked = false;
    bg.opacity = 255;
    bg.zOrder = 0;
    bg.image = QImage(32, 32, QImage::Format_ARGB32);
    bg.image.fill(Qt::red);

    CanvasLayer fg;
    fg.id = QStringLiteral("fg");
    fg.name = QStringLiteral("Foreground");
    fg.visible = true;
    fg.locked = false;
    fg.opacity = 255;
    fg.zOrder = 10;
    fg.image = QImage(32, 32, QImage::Format_ARGB32);
    fg.image.fill(Qt::transparent);

    canvas.setLayers({bg, fg}, 1);
    QCOMPARE(canvas.activeLayerIndex(), 1);

    // Initial composite is red everywhere
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::red));

    // Draw on active layer (Foreground) at (10, 10) with green
    canvas.setCurrentTool(PixelTool::Pencil);
    canvas.setPrimaryColor(Qt::green);
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(10 * canvas.zoom(), 10 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(10 * canvas.zoom(), 10 * canvas.zoom()), Qt::LeftButton);

    // Foreground cel must have green pixel at (10, 10)
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::green));
    // Background layer cel must still be red at (10, 10)
    QCOMPARE(canvas.layers().at(0).image.pixelColor(10, 10), QColor(Qt::red));
    // Composite must now show green at (10, 10)
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::green));

    // Test Undo on multi-layer cel
    canvas.undo();
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::transparent));
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::red));

    // Redo
    canvas.redo();
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::green));
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(Qt::green));

    // Test Write-Lock Protection
    canvas.setLayerLocked(1, true);
    QVERIFY(canvas.isLayerLocked());

    bool lockAttempted = false;
    connect(&canvas, &PixelCanvas::layerLockedAttempted, [&lockAttempted]() {
        lockAttempted = true;
    });

    // Attempt to draw on locked layer
    canvas.setPrimaryColor(Qt::blue);
    sendMouseEvent(&canvas, QEvent::MouseButtonPress, QPointF(15 * canvas.zoom(), 15 * canvas.zoom()), Qt::LeftButton);
    sendMouseEvent(&canvas, QEvent::MouseButtonRelease, QPointF(15 * canvas.zoom(), 15 * canvas.zoom()), Qt::LeftButton);
    QVERIFY(lockAttempted);
    // Pixel must NOT be modified
    QCOMPARE(canvas.activeLayerImage().pixelColor(15, 15), QColor(Qt::transparent));
}

void TestPixelEditor::testMultiLayerBlendModesAndOpacity()
{
    PixelCanvas canvas;

    CanvasLayer bg;
    bg.id = QStringLiteral("bg");
    bg.name = QStringLiteral("Background");
    bg.visible = true;
    bg.locked = false;
    bg.opacity = 255;
    bg.zOrder = 0;
    bg.image = QImage(32, 32, QImage::Format_ARGB32);
    bg.image.fill(QColor(255, 255, 255)); // White

    CanvasLayer fg;
    fg.id = QStringLiteral("fg");
    fg.name = QStringLiteral("MultiplyLayer");
    fg.visible = true;
    fg.locked = false;
    fg.opacity = 255;
    fg.zOrder = 10;
    fg.blendMode = QPainter::CompositionMode_Multiply;
    fg.image = QImage(32, 32, QImage::Format_ARGB32);
    fg.image.fill(QColor(255, 0, 0)); // Pure Red

    canvas.setLayers({bg, fg}, 1);

    // White multiplied by Red is Red
    QCOMPARE(canvas.image().pixelColor(10, 10).red(), 255);
    QCOMPARE(canvas.image().pixelColor(10, 10).green(), 0);
    QCOMPARE(canvas.image().pixelColor(10, 10).blue(), 0);

    // Change layer opacity to ~50% (128)
    canvas.setLayerOpacity(1, 128);
    QColor blended = canvas.image().pixelColor(10, 10);
    QCOMPARE(blended.red(), 255);
    QVERIFY(blended.green() > 100 && blended.green() < 155);

    // Toggle visibility off -> Background white should show through
    canvas.setLayerVisible(1, false);
    QCOMPARE(canvas.image().pixelColor(10, 10), QColor(255, 255, 255));
}

void TestPixelEditor::testMultiLayerSamplingAndFloodFill()
{
    PixelCanvas canvas;

    CanvasLayer bg;
    bg.id = QStringLiteral("lines");
    bg.name = QStringLiteral("Lines");
    bg.visible = true;
    bg.locked = false;
    bg.opacity = 255;
    bg.zOrder = 0;
    bg.image = QImage(32, 32, QImage::Format_ARGB32);
    bg.image.fill(Qt::transparent);
    // Draw a black square outline on Background from (5, 5) to (15, 15)
    for (int i = 5; i <= 15; ++i) {
        bg.image.setPixelColor(i, 5, Qt::black);
        bg.image.setPixelColor(i, 15, Qt::black);
        bg.image.setPixelColor(5, i, Qt::black);
        bg.image.setPixelColor(15, i, Qt::black);
    }

    CanvasLayer fg;
    fg.id = QStringLiteral("color");
    fg.name = QStringLiteral("Color");
    fg.visible = true;
    fg.locked = false;
    fg.opacity = 255;
    fg.zOrder = 10;
    fg.image = QImage(32, 32, QImage::Format_ARGB32);
    fg.image.fill(Qt::transparent);

    canvas.setLayers({bg, fg}, 1);

    // Mode A: sampleAllLayers = true
    canvas.setSampleAllLayers(true);
    // Flood fill on Foreground layer inside the outline at (10, 10) with yellow
    canvas.applyFloodFill(10, 10, Qt::yellow);

    // Inside the outline on Foreground should be yellow
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::yellow));
    // Outside the outline at (0, 0) Foreground should STILL BE TRANSPARENT (bounded by Background's lines!)
    QCOMPARE(canvas.activeLayerImage().pixelColor(0, 0), QColor(Qt::transparent));

    // Clear Foreground and test Mode B: sampleAllLayers = false
    canvas.undo();
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::transparent));

    canvas.setSampleAllLayers(false);
    // Flood fill at (10, 10) with cyan: since Foreground has no lines, it floods the ENTIRE layer!
    canvas.applyFloodFill(10, 10, Qt::cyan);
    QCOMPARE(canvas.activeLayerImage().pixelColor(10, 10), QColor(Qt::cyan));
    QCOMPARE(canvas.activeLayerImage().pixelColor(0, 0), QColor(Qt::cyan));
}

void TestPixelEditor::testMultiLayerOnionSkinRestricted()
{
    SpriteDocument doc;
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::red);
    QImage f1(32, 32, QImage::Format_ARGB32);
    f1.fill(Qt::green);
    doc.setFrames({f0, f1}, {SpriteBox(QRect(0, 0, 32, 32)), SpriteBox(QRect(0, 0, 32, 32))});

    SpriteLayer l1;
    l1.id = QStringLiteral("body");
    l1.name = QStringLiteral("Body");
    SpriteLayer l2;
    l2.id = QStringLiteral("sword");
    l2.name = QStringLiteral("Sword");
    doc.setLayers({l1, l2});

    SpriteCel c0_body{0, QStringLiteral("body"), 0, 0, 255, f0};
    QImage sword0(32, 32, QImage::Format_ARGB32);
    sword0.fill(Qt::transparent);
    sword0.setPixelColor(5, 5, Qt::blue);
    SpriteCel c0_sword{1, QStringLiteral("sword"), 0, 0, 255, sword0};
    doc.setFrameCels(0, {c0_body, c0_sword});

    SpriteCel c1_body{0, QStringLiteral("body"), 0, 0, 255, f1};
    QImage sword1(32, 32, QImage::Format_ARGB32);
    sword1.fill(Qt::transparent);
    sword1.setPixelColor(10, 10, Qt::blue);
    SpriteCel c1_sword{1, QStringLiteral("sword"), 0, 0, 255, sword1};
    doc.setFrameCels(1, {c1_body, c1_sword});

    PixelEditorDialog dlg(&doc, nullptr, 1);
    QVERIFY(dlg.canvas() != nullptr);
    QVERIFY(dlg.canvas()->hasLayers());

    // Switch active layer to "sword" (index 1)
    dlg.canvas()->setActiveLayerIndex(1);
    QCOMPARE(dlg.canvas()->activeLayerIndex(), 1);

    // Enable onion skin restricted to current layer
    dlg.canvas()->setOnionSkinCurrentLayerOnly(true);
    QVERIFY(dlg.canvas()->onionSkinCurrentLayerOnly());
}

void TestPixelEditor::testMultiLayerSessionSaveAndDocumentSync()
{
    SpriteDocument doc;
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::blue);
    doc.setFrames({f0}, {SpriteBox(QRect(0, 0, 32, 32))});

    PixelEditorDialog dlg(&doc, nullptr, 0);
    QVERIFY(dlg.layerStackWidget() != nullptr);

    // Add a new layer
    dlg.canvas()->addLayer(QStringLiteral("Hat"));
    QCOMPARE(dlg.canvas()->layerCount(), 2);
    QCOMPARE(dlg.canvas()->activeLayerIndex(), 1);

    // Draw on the new "Hat" layer at (16, 8) with yellow
    dlg.canvas()->setCurrentTool(PixelTool::Pencil);
    dlg.canvas()->setPrimaryColor(Qt::yellow);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonPress, QPointF(16 * dlg.canvas()->zoom(), 8 * dlg.canvas()->zoom()), Qt::LeftButton);
    sendMouseEvent(dlg.canvas(), QEvent::MouseButtonRelease, QPointF(16 * dlg.canvas()->zoom(), 8 * dlg.canvas()->zoom()), Qt::LeftButton);
    QCOMPARE(dlg.canvas()->activeLayerImage().pixelColor(16, 8), QColor(Qt::yellow));

    // Save/Apply changes
    dlg.applyButton()->click();

    // Document must now have 2 layers!
    QVERIFY(doc.hasLayers());
    QCOMPARE(doc.layerCount(), 2);
    QCOMPARE(doc.layers().at(1).name, QStringLiteral("Hat"));

    // Document cels for frame 0 must exist
    QVERIFY(doc.hasFrameCels(0));
    QList<SpriteCel> cels = doc.frameCels(0);
    QCOMPARE(cels.size(), 2);
    QCOMPARE(cels.at(1).image.pixelColor(16, 8), QColor(Qt::yellow));

    // Composite frame must combine both layers
    QImage comp = doc.compositeFrame(0);
    QCOMPARE(comp.pixelColor(16, 8), QColor(Qt::yellow));
    QCOMPARE(comp.pixelColor(0, 0), QColor(Qt::blue));
}



void TestPixelEditor::testErgonomicContextualPanels()
{
    SpriteDocument doc;
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::transparent);
    doc.setFrames({f0}, {SpriteBox(QRect(0, 0, 32, 32))});

    PixelEditorDialog dlg(&doc, nullptr, 0);

    // Verify filter button exists in toolbar
    QVERIFY(dlg.filtersButton() != nullptr);
    QCOMPARE(dlg.filtersButton()->objectName(), QStringLiteral("filtersButton"));

    // Verify contextual stack exists (0: Colors, 1: Eraser, 2: Marquee, 3: Eyedropper, 4: PolygonMesh)
    QStackedWidget *stack = dlg.findChild<QStackedWidget*>();
    QVERIFY(stack != nullptr);
    QCOMPARE(stack->count(), 5);

    // Initial tool is Pencil -> page 0 (Color studio)
    QCOMPARE(stack->currentIndex(), 0);

    // Switch to Eraser -> page 1 (Eraser options)
    dlg.canvas()->setCurrentTool(PixelTool::Eraser);
    // Trigger tool group click on eraser
    QList<QToolButton*> toolButtons = dlg.findChildren<QToolButton*>();
    for (QToolButton *btn : toolButtons) {
        if (btn->toolTip().contains(QLatin1String("Eraser"), Qt::CaseInsensitive)) {
            btn->click();
            break;
        }
    }
    QCOMPARE(stack->currentIndex(), 1);

    // Switch to SelectRect -> page 2 (Selection options)
    for (QToolButton *btn : toolButtons) {
        if (btn->toolTip().contains(QLatin1String("Marquee"), Qt::CaseInsensitive)) {
            btn->click();
            break;
        }
    }
    QCOMPARE(stack->currentIndex(), 2);

    // Switch to Eyedropper -> page 3 (Eyedropper options)
    for (QToolButton *btn : toolButtons) {
        if (btn->toolTip().contains(QLatin1String("Eyedropper"), Qt::CaseInsensitive)) {
            btn->click();
            break;
        }
    }
    QCOMPARE(stack->currentIndex(), 3);

    // Switch to PolygonEdit -> page 4 (Polygon mesh options)
    for (QToolButton *btn : toolButtons) {
        if (btn->text() == QStringLiteral("◆") || btn->toolTip().contains(QLatin1String("Polygonal"), Qt::CaseInsensitive) || btn->toolTip().contains(QLatin1String("CDT"), Qt::CaseInsensitive)) {
            btn->click();
            break;
        }
    }
    QCOMPARE(stack->currentIndex(), 4);

    // Switch to BucketFill -> page 0 (Color studio)
    for (QToolButton *btn : toolButtons) {
        if (btn->toolTip().contains(QLatin1String("Bucket"), Qt::CaseInsensitive)) {
            btn->click();
            break;
        }
    }
    QCOMPARE(stack->currentIndex(), 0);
}

void TestPixelEditor::testFilterAllFramesMultiLayerAndNavigation()
{
    // Setup document with 2 frames and 2 layers
    SpriteDocument doc;
    QImage f0(16, 16, QImage::Format_ARGB32);
    f0.fill(Qt::transparent);
    QImage f1(16, 16, QImage::Format_ARGB32);
    f1.fill(Qt::transparent);
    doc.setFrames({f0, f1}, {SpriteBox(QRect(0, 0, 16, 16)), SpriteBox(QRect(16, 0, 16, 16))});

    SpriteLayer l0;
    l0.id = QStringLiteral("bg");
    l0.name = QStringLiteral("Background");
    l0.zOrder = 0;
    l0.visible = true;

    SpriteLayer l1;
    l1.id = QStringLiteral("fg");
    l1.name = QStringLiteral("Foreground");
    l1.zOrder = 1;
    l1.visible = true;

    doc.setLayers({l0, l1});

    // Frame 0 cels: bg blue, fg solid red
    QImage bg0(16, 16, QImage::Format_ARGB32);
    bg0.fill(qRgba(0, 0, 255, 255));
    QImage fg0(16, 16, QImage::Format_ARGB32);
    fg0.fill(qRgba(255, 0, 0, 255));

    SpriteCel cel0_0;
    cel0_0.layerIndex = 0; cel0_0.layerId = l0.id; cel0_0.image = bg0;
    SpriteCel cel0_1;
    cel0_1.layerIndex = 1; cel0_1.layerId = l1.id; cel0_1.image = fg0;
    doc.setFrameCels(0, {cel0_0, cel0_1});

    // Frame 1 cels: bg blue, fg solid green
    QImage bg1(16, 16, QImage::Format_ARGB32);
    bg1.fill(qRgba(0, 0, 255, 255));
    QImage fg1(16, 16, QImage::Format_ARGB32);
    fg1.fill(qRgba(0, 255, 0, 255));

    SpriteCel cel1_0;
    cel1_0.layerIndex = 0; cel1_0.layerId = l0.id; cel1_0.image = bg1;
    SpriteCel cel1_1;
    cel1_1.layerIndex = 1; cel1_1.layerId = l1.id; cel1_1.image = fg1;
    doc.setFrameCels(1, {cel1_0, cel1_1});

    doc.recompositeAllFrames();
    doc.addAnimation(QStringLiteral("run"), {0, 1});

    QUndoStack undoStack;
    PixelEditorDialog dialog(&doc, &undoStack, 0);

    // 1. Verify Top Bar applyToAllFramesCheckBox exists and matches menu
    QCheckBox *checkAll = dialog.applyToAllFramesCheckBox();
    QVERIFY(checkAll != nullptr);
    QVERIFY(!checkAll->isChecked());

    QToolButton *filtersBtn = dialog.filtersButton();
    QVERIFY(filtersBtn != nullptr);
    QVERIFY(filtersBtn->menu() != nullptr);

    // Check first action in Filters menu is the Scope toggle action
    QList<QAction*> menuActs = filtersBtn->menu()->actions();
    QVERIFY(!menuActs.isEmpty());
    QAction *scopeAct = menuActs.first();
    QVERIFY(scopeAct->isCheckable());
    QVERIFY(!scopeAct->isChecked());

    // Toggle scope action in menu -> should update checkbox
    scopeAct->setChecked(true);
    QVERIFY(checkAll->isChecked());
    QVERIFY(dialog.isApplyToAllFramesEnabled());

    // 2. Mock filter that inverts pixels
    class MockInvertFilter : public FilterPlugin {
    public:
        QString id() const override { return QStringLiteral("mock_invert_filter"); }
        QString name() const override { return QStringLiteral("Mock Inverter"); }
        QString description() const override { return QStringLiteral("Invert RGB"); }
        QString category() const override { return QStringLiteral("Colors & Palettes"); }
        FilterModifierFlags modifierFlags() const override { return PixelModifier; }

        FilterDialogBase* createDialog(SpriteDocument *d, QUndoStack*, QWidget *parent) override {
            class MockInvertDlg : public FilterDialogBase {
            public:
                MockInvertDlg(SpriteDocument *doc, QWidget *p) : FilterDialogBase(doc, nullptr, p) {
                    QTimer::singleShot(0, this, &QDialog::accept);
                }
                void applyPreview() override {
                    if (!m_document) return;
                    QImage atlas = m_document->atlas();
                    atlas.invertPixels(QImage::InvertRgb);
                    m_document->setAtlas(atlas);
                    QList<QImage> fList;
                    for (const auto &b : m_document->boxes()) {
                        fList.append(atlas.copy(b.rect));
                    }
                    m_document->setFrames(fList, m_document->boxes());
                }
                QUndoCommand* createUndoCommand() override { return nullptr; }
                void resetDefaults() override {}
            };
            return new MockInvertDlg(d, parent);
        }
    };

    MockInvertFilter filter;

    // Target active layer = 1 (Foreground)
    dialog.canvas()->setActiveLayerIndex(1);
    QCOMPARE(dialog.canvas()->activeLayerIndex(), 1);

    // Apply filter to all frames of animation
    dialog.applyFilterToSession(&filter);

    // Frame 0 active cel was red (255, 0, 0) -> inverted should be cyan (0, 255, 255)
    QCOMPARE(dialog.canvas()->activeLayerImage().pixelColor(0, 0), QColor(0, 255, 255, 255));

    // 3. Navigate to Frame 1: must properly show inverted green -> magenta (255, 0, 255)
    dialog.loadFrame(1);
    dialog.canvas()->setActiveLayerIndex(1);
    QCOMPARE(dialog.canvas()->activeLayerImage().pixelColor(0, 0), QColor(255, 0, 255, 255));

    // Background layer on Frame 1 must NOT have been inverted (remains blue 0, 0, 255)
    dialog.canvas()->setActiveLayerIndex(0);
    QCOMPARE(dialog.canvas()->activeLayerImage().pixelColor(0, 0), QColor(0, 0, 255, 255));

    // 4. Validate and commit changes to document
    bool ok = dialog.applyChanges();
    QVERIFY(ok);

    // Verify document frame cels across all frames
    QList<SpriteCel> cels0 = doc.frameCels(0);
    QCOMPARE(cels0.size(), 2);
    QCOMPARE(cels0[0].image.pixelColor(0, 0), QColor(0, 0, 255, 255)); // bg blue
    QCOMPARE(cels0[1].image.pixelColor(0, 0), QColor(0, 255, 255, 255)); // fg cyan

    QList<SpriteCel> cels1 = doc.frameCels(1);
    QCOMPARE(cels1.size(), 2);
    QCOMPARE(cels1[0].image.pixelColor(0, 0), QColor(0, 0, 255, 255)); // bg blue
    QCOMPARE(cels1[1].image.pixelColor(0, 0), QColor(255, 0, 255, 255)); // fg magenta

    // Verify document recomposited frames
    QCOMPARE(doc.frame(0).pixelColor(0, 0), QColor(0, 255, 255, 255));
    QCOMPARE(doc.frame(1).pixelColor(0, 0), QColor(255, 0, 255, 255));
}

void TestPixelEditor::testPixelEditorPolygonMeshInteractiveEditingAndUndo()
{
    SpriteDocument doc;
    QImage f0(32, 32, QImage::Format_ARGB32);
    f0.fill(Qt::transparent);
    {
        QPainter p(&f0);
        p.fillRect(4, 4, 24, 24, Qt::red);
    }
    SpriteBox box(QRect(0, 0, 32, 32));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(4, 4) << QPointF(28, 4) << QPointF(28, 28) << QPointF(4, 28);
    box.vertices = box.polygon.toList();
    box.triangles = BentoPackGeometry::Triangulator::triangulateCDT(box.polygon, box.vertices);
    doc.setFrames({f0}, {box});

    QUndoStack docStack;
    PixelEditorDialog dlg(&doc, &docStack, 0);

    PixelCanvas *canvas = dlg.canvas();
    QVERIFY(canvas != nullptr);
    QVERIFY(canvas->hasPolygonMesh());
    QCOMPARE(canvas->polygonMesh().size(), 4);
    QCOMPARE(canvas->meshVertices().size(), 4);
    QVERIFY(!canvas->meshTriangles().isEmpty());

    // Switch to PolygonEdit tool
    canvas->setCurrentTool(PixelTool::PolygonEdit);
    QCOMPARE(canvas->currentTool(), PixelTool::PolygonEdit);

    // Switch edit mode to AddInterior
    canvas->setPolygonEditMode(PolygonEditMode::AddInterior);
    QCOMPARE(canvas->polygonEditMode(), PolygonEditMode::AddInterior);

    // Simulate mouse click inside polygon to add interior point at (16, 16)
    double zoom = canvas->zoom();
    QPoint imgOffset = canvas->imageOffset();
    QPoint widgetPos(static_cast<int>((16 + imgOffset.x()) * zoom),
                     static_cast<int>((16 + imgOffset.y()) * zoom));

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(widgetPos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &pressEvent);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(widgetPos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &releaseEvent);

    // Now mesh should have 5 vertices (4 boundary + 1 interior)
    QCOMPARE(canvas->meshVertices().size(), 5);
    QCOMPARE(canvas->meshVertices().last(), QPointF(16, 16));

    // Canvas undo should revert back to 4 vertices
    QVERIFY(canvas->canUndo());
    canvas->undo();
    QCOMPARE(canvas->meshVertices().size(), 4);

    // Canvas redo should restore the 5th vertex
    QVERIFY(canvas->canRedo());
    canvas->redo();
    QCOMPARE(canvas->meshVertices().size(), 5);

    // Test CutLine (Knife) tool mode
    canvas->setPolygonEditMode(PolygonEditMode::CutLine);
    QCOMPARE(canvas->polygonEditMode(), PolygonEditMode::CutLine);

    // Simulate drawing a cut line across the mesh from (6, 16) to (26, 16)
    QPoint cutStartWidget(static_cast<int>((6 + imgOffset.x()) * zoom),
                          static_cast<int>((16 + imgOffset.y()) * zoom));
    QPoint cutEndWidget(static_cast<int>((26 + imgOffset.x()) * zoom),
                        static_cast<int>((16 + imgOffset.y()) * zoom));

    QMouseEvent cutPress(QEvent::MouseButtonPress, QPointF(cutStartWidget), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &cutPress);
    QMouseEvent cutMove(QEvent::MouseMove, QPointF(cutEndWidget), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &cutMove);
    QMouseEvent cutRelease(QEvent::MouseButtonRelease, QPointF(cutEndWidget), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &cutRelease);

    // Cut line should have added additional vertices along the cut
    int vertexCountAfterCut = canvas->meshVertices().size();
    QVERIFY(vertexCountAfterCut > 5);
    QVERIFY(!canvas->meshTriangles().isEmpty());

    // Undo the cut line
    canvas->undo();
    QCOMPARE(canvas->meshVertices().size(), 5);

    // Redo the cut line
    canvas->redo();
    QCOMPARE(canvas->meshVertices().size(), vertexCountAfterCut);

    // Apply changes to document
    bool ok = dlg.applyChanges();
    QVERIFY(ok);

    // Document box must have the vertices and valid triangles
    QCOMPARE(doc.box(0).vertices.size(), vertexCountAfterCut);
    QVERIFY(!doc.box(0).triangles.isEmpty());
    QVERIFY(doc.box(0).vertices.contains(QPointF(16, 16)));

    // Document undo stack should allow undoing the EditSpritePixelsCommand
    QVERIFY(docStack.canUndo());
    docStack.undo();
    QCOMPARE(doc.box(0).vertices.size(), 4);

    // Document redo
    docStack.redo();
    QCOMPARE(doc.box(0).vertices.size(), vertexCountAfterCut);
}

void TestPixelEditor::testSmartMeshGenerationApplyToAllAnimationFrames()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    // Frame 0: 32x32 square
    QImage frame0(32, 32, QImage::Format_ARGB32);
    frame0.fill(Qt::transparent);
    frame0.setPixelColor(16, 16, Qt::black);
    for (int y = 4; y < 28; ++y) {
        for (int x = 4; x < 28; ++x) {
            frame0.setPixelColor(x, y, (x < 16) ? Qt::blue : Qt::cyan);
        }
    }

    // Frame 1: 32x32 square
    QImage frame1(32, 32, QImage::Format_ARGB32);
    frame1.fill(Qt::transparent);
    for (int y = 4; y < 28; ++y) {
        for (int x = 4; x < 28; ++x) {
            frame1.setPixelColor(x, y, (y < 16) ? Qt::red : Qt::yellow);
        }
    }

    QPolygonF poly;
    poly << QPointF(4, 4) << QPointF(28, 4) << QPointF(28, 28) << QPointF(4, 28);

    SpriteBox box0;
    box0.rect = QRect(0, 0, 32, 32);
    box0.hasPolygonMesh = true;
    box0.polygon = poly;
    box0.vertices = poly.toList();
    box0.triangles = BentoPackGeometry::Triangulator::triangulate(poly);

    SpriteBox box1;
    box1.rect = QRect(40, 0, 32, 32);
    box1.hasPolygonMesh = true;
    box1.polygon = poly;
    box1.vertices = poly.toList();
    box1.triangles = BentoPackGeometry::Triangulator::triangulate(poly);

    doc.setFrames({frame0, frame1}, {box0, box1});
    doc.setAtlas(atlas);
    doc.setAnimation(QStringLiteral("run"), {0, 1}, 10, true);

    QUndoStack docStack;
    PixelEditorDialog dlg(&doc, &docStack, 0);

    // Verify checkbox synchronization between header bar and contextual mesh panel
    QVERIFY(dlg.applyToAllFramesCheckBox() != nullptr);
    QVERIFY(dlg.meshApplyAllFramesCheckBox() != nullptr);
    QVERIFY(!dlg.isApplyToAllFramesEnabled());
    QVERIFY(!dlg.meshApplyAllFramesCheckBox()->isChecked());

    // Toggle contextual mesh checkbox -> should toggle main header checkbox
    dlg.meshApplyAllFramesCheckBox()->setChecked(true);
    QVERIFY(dlg.isApplyToAllFramesEnabled());
    QVERIFY(dlg.applyToAllFramesCheckBox()->isChecked());

    // Trigger CDT smart mesh generation on current frame (frame 0) with all frames enabled
    dlg.onGenerateSmartMeshRequested();

    // Verify current frame has CDT interior points
    QVERIFY(dlg.canvas()->meshVertices().size() > 4);
    QVERIFY(dlg.sessionModifiedVertices().contains(0));
    QVERIFY(dlg.sessionModifiedVertices().value(0).size() > 4);

    // Verify frame 1 (the other frame of "run" animation) also got CDT smart mesh generated!
    QVERIFY(dlg.sessionModifiedVertices().contains(1));
    QVERIFY(dlg.sessionModifiedVertices().value(1).size() > 4);
    QVERIFY(!dlg.sessionModifiedTriangles().value(1).isEmpty());

    // Test Reset Mesh to Outline with apply to all frames
    dlg.onResetMeshToOutlineRequested();

    // Frame 0 and Frame 1 should both be reset to 4 vertices (outline only)
    QCOMPARE(dlg.canvas()->meshVertices().size(), 4);
    QCOMPARE(dlg.sessionModifiedVertices().value(0).size(), 4);
    QCOMPARE(dlg.sessionModifiedVertices().value(1).size(), 4);

    // Apply changes to document
    QVERIFY(dlg.applyChanges());
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestPixelEditor tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_pixel_editor.moc"
