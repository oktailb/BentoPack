#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QUndoStack>
#include <QApplication>

#include "model/spritedocument.h"
#include "commands/commands.h"
#include "widgets/pixelcanvas.h"
#include "widgets/pixeleditordialog.h"

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
};

void TestPixelEditor::initTestCase()
{
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

    // Palettes are initialized in constructor
    // NES: 54 colors
    // SNES: 32 colors
    // Amiga: 32 colors
    // PC-Engine: 32 colors
    // Game Boy: 4 colors
    // Pico-8: 16 colors
    // Commodore 64: 16 colors
    // Verify preset values via switch logic
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::NES).size(), 54);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::SNES).size(), 32);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Amiga).size(), 32);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::PCEngine).size(), 32);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::GameBoy).size(), 4);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Pico8).size(), 16);
    QCOMPARE(dlg.getPresetPalette(PixelEditorDialog::Commodore64).size(), 16);
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

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestPixelEditor tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_pixel_editor.moc"
