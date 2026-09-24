#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QUndoStack>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

#include "model/spritedocument.h"
#include "packer/atlaspacker.h"
#include "packer/maxrectspacker.h"
#include "commands/commands.h"
#include "config/appconfig.h"
#include "license/licensemanager.h"
#include "license/integrityguard.h"

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // AtlasPacker tests (7 tests)
    void testAtlasPackerEmpty();
    void testAtlasPackerSingleFrame();
    void testAtlasPackerRowPacker();
    void testAtlasPackerGridPacker();
    void testAtlasPackerPowerOfTwoPacker();
    void testAtlasPackerPackIndices();
    void testAtlasPackerPadding();

    // MaxRects & Advanced Packing tests (M6)
    void testMaxRectsPackerBasic();
    void testMaxRectsHeuristics();
    void testAtlasPackerMaxRects();
    void testAtlasPackerPowerOfTwo();
    void testAtlasPackerDeduplication();
    void testAtlasPackerExtrude();
    void testAtlasPackerEfficiency();

    // SpriteDocument tests (8 tests)
    void testDocumentClearAndEmpty();
    void testDocumentInsertAndReplaceFrame();
    void testDocumentRemoveFramesMulti();
    void testDocumentReorderFrames();
    void testDocumentMergeFrames();
    void testDocumentComputeTrimmedRect();
    void testDocumentProjectNameMultiplatform();
    void testDocumentAnimationsCRUD();

    // Commands tests (8 tests)
    void testCommandAddSlice();
    void testCommandChangeBoxRect();
    void testCommandCreateAnimation();
    void testCommandReverseAnimation();
    void testCommandDeleteAnimation();
    void testCommandMergeFrames();
    void testCommandDeleteFrames();
    void testCommandEraseAtlasPixels();

    // Multiplatform & System tests (3 tests)
    void testMultiplatformPathSeparators();
    void testMultiplatformImageFormats();
    void testMultiplatformAppConfigLocations();

    // Pivot tests (3 tests)
    void testPivotPresetsCalculation();
    void testDocumentPivotMethods();
    void testCommandChangePivot();
    void testLicenseComplianceAndWatermarking();
    void testIntegrityGuardAndForensicWatermarking();
};

void TestCore::initTestCase()
{
}

void TestCore::cleanupTestCase()
{
}

// =============================================================================
// AtlasPacker Tests
// =============================================================================

void TestCore::testAtlasPackerEmpty()
{
    QList<QImage> emptyFrames;
    AtlasPackResult res = AtlasPacker::pack(emptyFrames);
    QVERIFY(!res.success);
    QVERIFY(res.atlas.isNull());
    QVERIFY(res.frameRects.isEmpty());
}

void TestCore::testAtlasPackerSingleFrame()
{
    QImage img(32, 24, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::red);

    AtlasPackResult res = AtlasPacker::pack({img}, 4, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 1);
    QCOMPARE(res.frameRects[0].width(), 32);
    QCOMPARE(res.frameRects[0].height(), 24);
    QVERIFY(res.atlas.width() >= 32 + 8);
    QVERIFY(res.atlas.height() >= 24 + 8);
}

void TestCore::testAtlasPackerRowPacker()
{
    QList<QImage> frames;
    QList<QSize> sizes = { QSize(16, 16), QSize(32, 48), QSize(64, 20), QSize(20, 60) };
    QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::yellow };

    for (int i = 0; i < sizes.size(); ++i) {
        QImage img(sizes[i], QImage::Format_ARGB32_Premultiplied);
        img.fill(colors[i]);
        frames.append(img);
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), frames.size());

    // 1. All rects fit strictly inside atlas boundaries
    for (int i = 0; i < res.frameRects.size(); ++i) {
        QVERIFY(res.atlas.rect().contains(res.frameRects[i]));
        QCOMPARE(res.frameRects[i].size(), sizes[i]);
    }

    // 2. No frame rects overlap with each other
    for (int i = 0; i < res.frameRects.size(); ++i) {
        for (int j = i + 1; j < res.frameRects.size(); ++j) {
            QVERIFY(!res.frameRects[i].intersects(res.frameRects[j]));
        }
    }

    // 3. Pixel fidelity: sampled colors match
    for (int i = 0; i < res.frameRects.size(); ++i) {
        QPoint samplePoint = res.frameRects[i].center();
        QColor atlasColor = res.atlas.pixelColor(samplePoint);
        QCOMPARE(atlasColor.name(), colors[i].name());
    }
}

void TestCore::testAtlasPackerGridPacker()
{
    QList<QImage> frames;
    for (int i = 0; i < 6; ++i) {
        QImage img(24, 24, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::cyan);
        frames.append(img);
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::GridPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 6);

    for (int i = 0; i < res.frameRects.size(); ++i) {
        QVERIFY(res.atlas.rect().contains(res.frameRects[i]));
        for (int j = i + 1; j < res.frameRects.size(); ++j) {
            QVERIFY(!res.frameRects[i].intersects(res.frameRects[j]));
        }
    }
}

void TestCore::testAtlasPackerPowerOfTwoPacker()
{
    QList<QImage> frames;
    for (int i = 0; i < 5; ++i) {
        QImage img(35, 45, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::magenta);
        frames.append(img);
    }

    AtlasPackResult res = AtlasPacker::pack(frames, 2, AtlasPacker::PowerOfTwoPacker);
    QVERIFY(res.success);

    // Verify width and height are strictly powers of two (crucial for GPUs across platforms)
    int w = res.atlas.width();
    int h = res.atlas.height();
    QVERIFY(w > 0 && (w & (w - 1)) == 0);
    QVERIFY(h > 0 && (h & (h - 1)) == 0);
}

void TestCore::testAtlasPackerPackIndices()
{
    QList<QImage> frames;
    for (int i = 0; i < 4; ++i) {
        QImage img(10 * (i + 1), 10 * (i + 1), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        frames.append(img);
    }

    // Pack only frame indices 1 and 3 (sizes 20x20 and 40x40)
    AtlasPackResult res = AtlasPacker::packIndices(frames, {1, 3}, 2);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 2);
    QCOMPARE(res.frameRects[0].size(), QSize(20, 20));
    QCOMPARE(res.frameRects[1].size(), QSize(40, 40));

    // Empty indices
    AtlasPackResult emptyRes = AtlasPacker::packIndices(frames, {}, 2);
    QVERIFY(!emptyRes.success);
}

void TestCore::testAtlasPackerPadding()
{
    QImage img(20, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);

    int padding = 8;
    AtlasPackResult res = AtlasPacker::pack({img, img}, padding, AtlasPacker::RowPacker);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 2);

    // Margins from atlas borders must be at least padding
    QVERIFY(res.frameRects[0].left() >= padding);
    QVERIFY(res.frameRects[0].top() >= padding);

    // Spacing between adjacent frames must be at least padding
    if (res.frameRects[0].top() == res.frameRects[1].top()) {
        int gap = res.frameRects[1].left() - res.frameRects[0].right();
        QVERIFY(gap >= padding);
    }
}

void TestCore::testMaxRectsPackerBasic()
{
    MaxRectsPacker packer;
    packer.init(128, 128);

    QList<QSize> sizes = { QSize(32, 32), QSize(64, 32), QSize(16, 64), QSize(48, 48) };
    QList<QRect> placed = packer.insert(sizes, MaxRectsHeuristic::BestShortSideFit);

    QCOMPARE(placed.size(), sizes.size());

    // 1. All rects fit inside 128x128
    QRect binRect(0, 0, 128, 128);
    for (int i = 0; i < placed.size(); ++i) {
        QVERIFY(binRect.contains(placed[i]));
        QCOMPARE(placed[i].size(), sizes[i]);
    }

    // 2. No rects overlap
    for (int i = 0; i < placed.size(); ++i) {
        for (int j = i + 1; j < placed.size(); ++j) {
            QVERIFY(!placed[i].intersects(placed[j]));
        }
    }

    // 3. Occupancy
    double occupancy = packer.occupancy();
    int usedArea = 32 * 32 + 64 * 32 + 16 * 64 + 48 * 48;
    double expectedOccupancy = static_cast<double>(usedArea) / (128.0 * 128.0);
    QVERIFY(qAbs(occupancy - expectedOccupancy) < 0.0001);
}

void TestCore::testMaxRectsHeuristics()
{
    QList<MaxRectsHeuristic> heuristics = {
        MaxRectsHeuristic::BestShortSideFit,
        MaxRectsHeuristic::BestLongSideFit,
        MaxRectsHeuristic::BestAreaFit,
        MaxRectsHeuristic::BottomLeft,
        MaxRectsHeuristic::ContactPoint
    };

    QList<QSize> sizes = { QSize(20, 30), QSize(40, 20), QSize(30, 30), QSize(15, 25), QSize(50, 20) };

    for (MaxRectsHeuristic h : heuristics) {
        MaxRectsPacker packer;
        packer.init(128, 128);
        QList<QRect> placed = packer.insert(sizes, h);
        QCOMPARE(placed.size(), sizes.size());

        for (int i = 0; i < placed.size(); ++i) {
            QVERIFY(QRect(0, 0, 128, 128).contains(placed[i]));
            for (int j = i + 1; j < placed.size(); ++j) {
                QVERIFY(!placed[i].intersects(placed[j]));
            }
        }
    }
}

void TestCore::testAtlasPackerMaxRects()
{
    QList<QImage> frames;
    QList<QSize> sizes = { QSize(24, 24), QSize(48, 32), QSize(32, 64), QSize(16, 16) };
    QList<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::yellow };

    for (int i = 0; i < sizes.size(); ++i) {
        QImage img(sizes[i], QImage::Format_ARGB32_Premultiplied);
        img.fill(colors[i]);
        frames.append(img);
    }

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.heuristic = MaxRectsHeuristic::BestShortSideFit;
    opts.padding = 2;
    opts.borderPadding = 2;

    AtlasPackResult res = AtlasPacker::pack(frames, opts);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), frames.size());

    // Check containment and non-overlapping
    for (int i = 0; i < res.frameRects.size(); ++i) {
        QVERIFY(res.atlas.rect().contains(res.frameRects[i]));
        QCOMPARE(res.frameRects[i].size(), sizes[i]);
        for (int j = i + 1; j < res.frameRects.size(); ++j) {
            QVERIFY(!res.frameRects[i].intersects(res.frameRects[j]));
        }
        // Pixel fidelity
        QCOMPARE(res.atlas.pixelColor(res.frameRects[i].center()).name(), colors[i].name());
    }
}

void TestCore::testAtlasPackerPowerOfTwo()
{
    QList<QImage> frames;
    for (int i = 0; i < 3; ++i) {
        QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::cyan);
        frames.append(img);
    }

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.powerOfTwo = true;
    opts.forceSquare = true;

    AtlasPackResult res = AtlasPacker::pack(frames, opts);
    QVERIFY(res.success);

    int w = res.atlas.width();
    int h = res.atlas.height();
    QVERIFY(w > 0 && (w & (w - 1)) == 0);
    QVERIFY(h > 0 && (h & (h - 1)) == 0);
    QCOMPARE(w, h);
}

void TestCore::testAtlasPackerDeduplication()
{
    // Create 4 frames: frames 0 and 2 identical red, frame 1 green, frame 3 blue
    QImage red(20, 20, QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);

    QImage green(20, 20, QImage::Format_ARGB32_Premultiplied);
    green.fill(Qt::green);

    QImage blue(20, 20, QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);

    QList<QImage> frames = { red, green, red, blue };

    // With deduplication enabled
    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.deduplicate = true;

    AtlasPackResult res = AtlasPacker::pack(frames, opts);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 4);
    QCOMPARE(res.uniqueFramesCount, 3);
    QCOMPARE(res.duplicateMapping.size(), 4);
    QCOMPARE(res.duplicateMapping[0], 0);
    QCOMPARE(res.duplicateMapping[1], 1);
    QCOMPARE(res.duplicateMapping[2], 0); // frame 2 mapped to frame 0
    QCOMPARE(res.duplicateMapping[3], 2); // frame 3 mapped to unique frame 2

    // Frame rect 0 and frame rect 2 must be identical in atlas
    QCOMPARE(res.frameRects[0], res.frameRects[2]);

    // With deduplication disabled
    opts.deduplicate = false;
    AtlasPackResult resNoDedup = AtlasPacker::pack(frames, opts);
    QVERIFY(resNoDedup.success);
    QCOMPARE(resNoDedup.uniqueFramesCount, 4);
    QVERIFY(resNoDedup.frameRects[0] != resNoDedup.frameRects[2]);
}

void TestCore::testAtlasPackerExtrude()
{
    // 8x8 frame filled with solid yellow
    QImage img(8, 8, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::yellow);

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;
    opts.padding = 4;
    opts.borderPadding = 4;
    opts.extrude = 1;

    AtlasPackResult res = AtlasPacker::pack({img}, opts);
    QVERIFY(res.success);
    QCOMPARE(res.frameRects.size(), 1);

    QRect r = res.frameRects[0];
    QString yellowName = QColor(Qt::yellow).name();
    // Check pixel directly left of frame (extruded pixel)
    QPoint leftPixel(r.left() - 1, r.center().y());
    QCOMPARE(res.atlas.pixelColor(leftPixel).name(), yellowName);

    // Check pixel directly above frame (extruded pixel)
    QPoint topPixel(r.center().x(), r.top() - 1);
    QCOMPARE(res.atlas.pixelColor(topPixel).name(), yellowName);

    // Check pixel directly right of frame
    QPoint rightPixel(r.right() + 1, r.center().y());
    QCOMPARE(res.atlas.pixelColor(rightPixel).name(), yellowName);

    // Check pixel directly bottom of frame
    QPoint bottomPixel(r.center().x(), r.bottom() + 1);
    QCOMPARE(res.atlas.pixelColor(bottomPixel).name(), yellowName);
}

void TestCore::testAtlasPackerEfficiency()
{
    QImage img(50, 50, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    AtlasPacker::PackOptions opts;
    opts.algorithm = AtlasPacker::MaxRects;

    AtlasPackResult res = AtlasPacker::pack({img}, opts);
    QVERIFY(res.success);
    QVERIFY(res.efficiency > 0.0 && res.efficiency <= 100.0);
}

// =============================================================================
// SpriteDocument Tests
// =============================================================================

void TestCore::testDocumentClearAndEmpty()
{
    SpriteDocument doc;
    QVERIFY(doc.isEmpty());

    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 50, 50));
    doc.setAnimation(QStringLiteral("idle"), {0}, 12, true);
    doc.setFilePath(QStringLiteral("/path/to/project.png"));

    QVERIFY(!doc.isEmpty());
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.animationNames().size(), 1);

    QSignalSpy spyReset(&doc, &SpriteDocument::documentReset);
    doc.clear();

    QVERIFY(doc.isEmpty());
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(doc.boxes().size(), 0);
    QCOMPARE(doc.animationNames().size(), 0);
    QVERIFY(doc.filePath().isEmpty());
    QCOMPARE(spyReset.count(), 1);
}

void TestCore::testDocumentInsertAndReplaceFrame()
{
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    doc.setAtlas(atlas);

    QImage img1(20, 20, QImage::Format_ARGB32);
    QImage img2(30, 40, QImage::Format_ARGB32);
    QImage imgInsert(60, 25, QImage::Format_ARGB32);
    QImage imgReplace(80, 15, QImage::Format_ARGB32);

    doc.addFrame(img1);
    doc.addFrame(img2);
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.maxFrameWidth(), 30);
    QCOMPARE(doc.maxFrameHeight(), 40);

    // Insert at index 1
    SpriteBox insertBox;
    insertBox.rect = QRect(0, 0, 60, 25);
    doc.insertFrame(1, imgInsert, insertBox);
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.frame(1).width(), 60);
    QCOMPARE(doc.maxFrameWidth(), 60);

    // Replace at index 0
    SpriteBox replaceBox;
    replaceBox.rect = QRect(0, 0, 80, 15);
    doc.replaceFrame(0, imgReplace, replaceBox);
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.frame(0).width(), 80);
    QCOMPARE(doc.maxFrameWidth(), 80);
}

void TestCore::testDocumentRemoveFramesMulti()
{
    SpriteDocument doc;
    for (int i = 0; i < 4; ++i) {
        QImage img(10 * (i + 1), 10, QImage::Format_ARGB32);
        SpriteBox box;
        box.rect = QRect(i * 10, 0, 10 * (i + 1), 10);
        box.index = i;
        doc.addFrame(img, box);
    }
    QCOMPARE(doc.frameCount(), 4);

    // Remove frames at discontinuous indices {1, 3}
    doc.removeFrames({1, 3});
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.frame(0).width(), 10);
    QCOMPARE(doc.frame(1).width(), 30);
}

void TestCore::testDocumentReorderFrames()
{
    SpriteDocument doc;
    for (int i = 0; i < 3; ++i) {
        QImage img(10 * (i + 1), 10, QImage::Format_ARGB32);
        SpriteBox box;
        box.rect = QRect(i * 10, 0, 10 * (i + 1), 10);
        box.index = i;
        doc.addFrame(img, box);
    }

    doc.setAnimation(QStringLiteral("run"), {0, 1, 2}, 12, true);

    // New order: index 2 becomes 0, index 0 becomes 1, index 1 becomes 2
    doc.reorderFrames({2, 0, 1});
    QCOMPARE(doc.frame(0).width(), 30);
    QCOMPARE(doc.frame(1).width(), 10);
    QCOMPARE(doc.frame(2).width(), 20);

    // Animation frame indices mapped automatically
    QCOMPARE(doc.animation(QStringLiteral("run")).frameIndices, (QList<int>{1, 2, 0}));
}

void TestCore::testDocumentMergeFrames()
{
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    doc.addSlice(QRect(10, 10, 20, 20)); // index 0
    doc.addSlice(QRect(40, 10, 20, 20)); // index 1

    QCOMPARE(doc.frameCount(), 2);

    // Merge index 0 into index 1
    doc.mergeFrames(0, 1);
    QCOMPARE(doc.frameCount(), 1);

    // The merged box should unite QRect(10, 10, 20, 20) and QRect(40, 10, 20, 20) => QRect(10, 10, 50, 20)
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 50, 20));
}

void TestCore::testDocumentComputeTrimmedRect()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    // Draw opaque content at (30, 20, 40, 50)
    for (int y = 20; y < 70; ++y) {
        for (int x = 30; x < 70; ++x) {
            atlas.setPixelColor(x, y, QColor(255, 0, 0, 255));
        }
    }
    doc.setAtlas(atlas);

    // Add a larger slice that encompasses the content with transparent padding
    doc.addSlice(QRect(10, 5, 80, 90));

    // Trim with alphaThreshold = 1
    QRect trimmed = doc.computeTrimmedRect(0, 1);
    QCOMPARE(trimmed, QRect(30, 20, 40, 50));

    // Fully transparent slice returns original rect
    doc.addSlice(QRect(0, 0, 15, 15));
    QRect trimmedEmpty = doc.computeTrimmedRect(1, 1);
    QCOMPARE(trimmedEmpty, QRect(0, 0, 15, 15));
}

void TestCore::testDocumentProjectNameMultiplatform()
{
    SpriteDocument doc;
    QCOMPARE(doc.projectName(), QStringLiteral("untitled"));

    // 1. Relative path syntax
    doc.setFilePath(QStringLiteral("sprites/hero_idle.png"));
    QCOMPARE(doc.projectName(), QStringLiteral("hero_idle"));

    // 2. Linux / Unix path syntax
    doc.setFilePath(QStringLiteral("/home/developer/games/assets/monster_walk.tres"));
    QCOMPARE(doc.projectName(), QStringLiteral("monster_walk"));

    // 3. Apple macOS path syntax
    doc.setFilePath(QStringLiteral("/Users/designer/Desktop/boss_attack.gif"));
    QCOMPARE(doc.projectName(), QStringLiteral("boss_attack"));

    // 4. Haiku path syntax
    doc.setFilePath(QStringLiteral("/boot/home/config/settings/particles.json"));
    QCOMPARE(doc.projectName(), QStringLiteral("particles"));

    // 5. Multi-dot extension handling
    doc.setFilePath(QStringLiteral("archive.sheet.v1.0.png"));
    QCOMPARE(doc.projectName(), QStringLiteral("archive.sheet.v1.0"));
}

void TestCore::testDocumentAnimationsCRUD()
{
    SpriteDocument doc;
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));

    QSignalSpy spyAnim(&doc, &SpriteDocument::animationsChanged);

    doc.setAnimation(QStringLiteral("walk"), {0, 1, 2}, 16, true);
    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
    QCOMPARE(doc.animation(QStringLiteral("walk")).fps, 16);
    QVERIFY(doc.animation(QStringLiteral("walk")).loop);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2}));
    QVERIFY(spyAnim.count() >= 1);

    // Reversing animation frames
    doc.reverseAnimationFrames(QStringLiteral("walk"));
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{2, 1, 0}));

    // Removing animation
    doc.removeAnimation(QStringLiteral("walk"));
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));
}

// =============================================================================
// Commands Undo/Redo Tests
// =============================================================================

void TestCore::testCommandAddSlice()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    QUndoStack undoStack;
    undoStack.push(new AddSliceCommand(&doc, QRect(10, 10, 30, 30)));
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 30, 30));

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 0);

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 30, 30));
}

void TestCore::testCommandChangeBoxRect()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    undoStack.push(new ChangeBoxRectCommand(&doc, 0, QRect(10, 10, 20, 20), QRect(15, 25, 40, 50)));
    QCOMPARE(doc.box(0).rect, QRect(15, 25, 40, 50));

    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));

    undoStack.redo();
    QCOMPARE(doc.box(0).rect, QRect(15, 25, 40, 50));
}

void TestCore::testCommandCreateAnimation()
{
    SpriteDocument doc;
    QUndoStack undoStack;

    undoStack.push(new CreateAnimationCommand(&doc, QStringLiteral("attack"), {0, 1, 2}, 24));
    QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
    QCOMPARE(doc.animation(QStringLiteral("attack")).fps, 24);

    undoStack.undo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("attack")));

    undoStack.redo();
    QVERIFY(doc.hasAnimation(QStringLiteral("attack")));
}

void TestCore::testCommandReverseAnimation()
{
    SpriteDocument doc;
    doc.setAnimation(QStringLiteral("idle"), {0, 1, 2, 3}, 12, true);

    QUndoStack undoStack;
    undoStack.push(new ReverseAnimationCommand(&doc, QStringLiteral("idle")));
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{3, 2, 1, 0}));

    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{0, 1, 2, 3}));

    undoStack.redo();
    QCOMPARE(doc.animation(QStringLiteral("idle")).frameIndices, (QList<int>{3, 2, 1, 0}));
}

void TestCore::testCommandDeleteAnimation()
{
    SpriteDocument doc;
    doc.setAnimation(QStringLiteral("die"), {4, 5}, 8, false);

    QUndoStack undoStack;
    undoStack.push(new DeleteAnimationCommand(&doc, QStringLiteral("die")));
    QVERIFY(!doc.hasAnimation(QStringLiteral("die")));

    undoStack.undo();
    QVERIFY(doc.hasAnimation(QStringLiteral("die")));
    QCOMPARE(doc.animation(QStringLiteral("die")).fps, 8);
    QVERIFY(!doc.animation(QStringLiteral("die")).loop);

    undoStack.redo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("die")));
}

void TestCore::testCommandMergeFrames()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(5, 5, 20, 20));  // index 0
    doc.addSlice(QRect(30, 5, 20, 20)); // index 1

    QUndoStack undoStack;
    undoStack.push(new MergeFramesCommand(&doc, 0, 1));
    QCOMPARE(doc.frameCount(), 1);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(doc.box(0).rect, QRect(5, 5, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(30, 5, 20, 20));

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
}

void TestCore::testCommandDeleteFrames()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 10, 10));
    doc.addSlice(QRect(20, 0, 10, 10));
    doc.addSlice(QRect(40, 0, 10, 10));

    QUndoStack undoStack;
    undoStack.push(new DeleteFramesCommand(&doc, {0, 2}));
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(20, 0, 10, 10));

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 10, 10));
    QCOMPARE(doc.box(1).rect, QRect(20, 0, 10, 10));
    QCOMPARE(doc.box(2).rect, QRect(40, 0, 10, 10));

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 1);
}

void TestCore::testCommandEraseAtlasPixels()
{
    SpriteDocument doc;
    QImage atlas(50, 50, QImage::Format_ARGB32);
    atlas.fill(QColor(255, 128, 0, 255));
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    undoStack.push(new EraseAtlasPixelsCommand(&doc, {0}));
    QCOMPARE(doc.frameCount(), 0);
    // Erased region is transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
    // Non-erased region is opaque
    QCOMPARE(qAlpha(doc.atlas().pixel(2, 2)), 255);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 255);

    undoStack.redo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
}

// =============================================================================
// Multiplatform & System Tests
// =============================================================================

void TestCore::testMultiplatformPathSeparators()
{
    // Ensure mixing forward slashes and backslashes is normalized properly across OS
    QString mixedPath = QStringLiteral("assets/sprites\\level1/hero.png");
    QString unifiedPath = mixedPath;
    unifiedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QVERIFY(!unifiedPath.contains(QLatin1Char('\\')));
    QCOMPARE(QFileInfo(unifiedPath).fileName(), QStringLiteral("hero.png"));

    // Case insensitivity extension check simulation
    QStringList validExts = { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("gif"), QStringLiteral("json") };
    QString fileUpper = QStringLiteral("MY_SPRITE.PNG");
    QString ext = QFileInfo(fileUpper).suffix().toLower();
    QVERIFY(validExts.contains(ext));
}

void TestCore::testMultiplatformImageFormats()
{
    // Validate 32-bit ARGB memory alignment and alpha fidelity across architectures
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(qRgba(120, 200, 50, 180));

    // Verify alpha is preserved exactly
    QCOMPARE(qAlpha(img.pixel(2, 2)), 180);
    QCOMPARE(qRed(img.pixel(2, 2)), 120);
    QCOMPARE(qGreen(img.pixel(2, 2)), 200);
    QCOMPARE(qBlue(img.pixel(2, 2)), 50);

    // Scanline pointer memory step is 4 bytes per pixel
    const uchar *scan0 = img.constScanLine(0);
    const uchar *scan1 = img.constScanLine(1);
    QCOMPARE(scan1 - scan0, 4 * sizeof(QRgb));
}

void TestCore::testMultiplatformAppConfigLocations()
{
    AppConfig &cfg = AppConfig::instance();
    QString path = cfg.configFilePath();
    QVERIFY(!path.isEmpty());

    // Saving and loading in temporary cross-platform directory
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString testCfgPath = tempDir.filePath(QStringLiteral("config_portable.json"));

    cfg.setConfigFilePath(testCfgPath);
    cfg.atlas().zoomStep = 1.35;
    QVERIFY(cfg.save());
    QVERIFY(QFile::exists(testCfgPath));

    cfg.resetToDefaults();
    QCOMPARE(cfg.atlas().zoomStep, 1.15);

    QVERIFY(cfg.load());
    QCOMPARE(cfg.atlas().zoomStep, 1.35);

    // Cleanup config path
    cfg.setConfigFilePath(QString());
}

void TestCore::testPivotPresetsCalculation()
{
    QSize sz(100, 200);

    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::TopLeft, sz), QPoint(0, 0));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::TopCenter, sz), QPoint(50, 0));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::TopRight, sz), QPoint(100, 0));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::CenterLeft, sz), QPoint(0, 100));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::Center, sz), QPoint(50, 100));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::CenterRight, sz), QPoint(100, 100));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::BottomLeft, sz), QPoint(0, 200));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::BottomCenter, sz), QPoint(50, 200));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::BottomRight, sz), QPoint(100, 200));
    QCOMPARE(SpriteBox::calculatePresetPivot(PivotPreset::Custom, sz), QPoint(50, 200));
}

void TestCore::testDocumentPivotMethods()
{
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Frame 0: 40x60
    int idx0 = doc.addSlice(QRect(0, 0, 40, 60));
    // Frame 1: 50x80
    int idx1 = doc.addSlice(QRect(50, 0, 50, 80));

    // Default pivot should be BottomCenter: w/2, h
    QCOMPARE(doc.boxPivot(idx0), QPoint(20, 60));
    QCOMPARE(doc.boxHasCustomPivot(idx0), false);
    QCOMPARE(doc.boxPivot(idx1), QPoint(25, 80));
    QCOMPARE(doc.boxHasCustomPivot(idx1), false);

    // Set custom pivot on frame 0
    QSignalSpy spyPivot(&doc, &SpriteDocument::boxPivotChanged);
    doc.setBoxPivot(idx0, QPoint(15, 45), true);
    QCOMPARE(spyPivot.count(), 1);
    QCOMPARE(doc.boxPivot(idx0), QPoint(15, 45));
    QCOMPARE(doc.boxHasCustomPivot(idx0), true);

    // Apply preset Center to frame 1
    doc.applyPivotPreset({idx1}, PivotPreset::Center);
    QCOMPARE(doc.boxPivot(idx1), QPoint(25, 40));
    QCOMPARE(doc.boxHasCustomPivot(idx1), true);

    // Set multi-boxes pivot
    doc.setBoxesPivot({idx0, idx1}, QPoint(10, 10), true);
    QCOMPARE(doc.boxPivot(idx0), QPoint(10, 10));
    QCOMPARE(doc.boxPivot(idx1), QPoint(10, 10));

    // Envelope calculation
    doc.setAnimation(QStringLiteral("test_anim"), {idx0, idx1}, 12, true);
    QRect env = doc.computeAnimationEnvelope(QStringLiteral("test_anim"));
    QCOMPARE(env.x(), 10);
    QCOMPARE(env.y(), 10);
    QCOMPARE(env.width(), 50);
    QCOMPARE(env.height(), 80);
}

void TestCore::testCommandChangePivot()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 30, 40));

    QUndoStack undoStack;
    QPoint oldPivot = doc.boxPivot(idx);
    QPoint newPivot(12, 34);

    undoStack.push(new ChangePivotCommand(&doc, idx, oldPivot, newPivot, true));
    QCOMPARE(doc.boxPivot(idx), newPivot);
    QCOMPARE(doc.boxHasCustomPivot(idx), true);

    undoStack.undo();
    QCOMPARE(doc.boxPivot(idx), oldPivot);

    undoStack.redo();
    QCOMPARE(doc.boxPivot(idx), newPivot);
}

void TestCore::testLicenseComplianceAndWatermarking()
{
    bool isComm = SpriteStudio::LicenseManager::isCommercial();

    // Test both GUI and CLI tool types
    for (SpriteStudio::ToolType t : {SpriteStudio::ToolType::GUI, SpriteStudio::ToolType::CLI}) {
        SpriteStudio::LicenseManager::setToolType(t);
        const QString toolStr = (t == SpriteStudio::ToolType::CLI) ? QStringLiteral("CLI") : QStringLiteral("GUI");

        if (isComm) {
            // 1. Commercial Edition assertions
            QCOMPARE(SpriteStudio::LicenseManager::edition(), SpriteStudio::Edition::Commercial);
            QCOMPARE(SpriteStudio::LicenseManager::editionName(), QStringLiteral("Commercial Edition"));

            // 2. Compliance metadata
            QMap<QString, QString> meta = SpriteStudio::LicenseManager::complianceMetadata();
            QCOMPARE(meta.value(QStringLiteral("Generator")), QStringLiteral("SpriteStudio %1").arg(toolStr));
            QCOMPARE(meta.value(QStringLiteral("X-SpriteStudio-Tool")), toolStr);
            QVERIFY(!meta.contains(QStringLiteral("X-SpriteStudio-License")));

            // 3. Image watermarking (clean in commercial)
            QImage testImg(32, 32, QImage::Format_ARGB32_Premultiplied);
            testImg.fill(Qt::blue);
            SpriteStudio::LicenseManager::applyWatermark(testImg);
            QCOMPARE(testImg.text(QStringLiteral("Generator")), QStringLiteral("SpriteStudio %1").arg(toolStr));
            QCOMPARE(testImg.text(QStringLiteral("X-SpriteStudio-Tool")), toolStr);
            QVERIFY(testImg.text(QStringLiteral("X-SpriteStudio-License")).isEmpty());

            // 4. JSON metadata (clean in commercial)
            QJsonObject jsonMeta;
            jsonMeta["version"] = "1.0";
            SpriteStudio::LicenseManager::applyWatermark(jsonMeta);
            QCOMPARE(jsonMeta["app"].toString(), QStringLiteral("SpriteStudio %1").arg(toolStr));
            QCOMPARE(jsonMeta["tool"].toString(), toolStr);
            QVERIFY(!jsonMeta.contains(QStringLiteral("license")));

            // 5. Header comment
            QString header = SpriteStudio::LicenseManager::watermarkHeaderComment();
            QVERIFY(!header.contains(QStringLiteral("Community Edition")));
        } else {
            // 1. Community Edition assertions
            QCOMPARE(SpriteStudio::LicenseManager::edition(), SpriteStudio::Edition::Community);
            QCOMPARE(SpriteStudio::LicenseManager::editionName(), QStringLiteral("Community Edition"));

            // 2. Compliance metadata dictionary
            QMap<QString, QString> meta = SpriteStudio::LicenseManager::complianceMetadata();
            QCOMPARE(meta.value(QStringLiteral("Generator")), QStringLiteral("SpriteStudio %1 Community Edition").arg(toolStr));
            QCOMPARE(meta.value(QStringLiteral("X-SpriteStudio-Tool")), toolStr);
            QCOMPARE(meta.value(QStringLiteral("X-SpriteStudio-License")), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));

            // 3. Image watermarking (PNG tEXt chunk metadata)
            QImage testImg(32, 32, QImage::Format_ARGB32_Premultiplied);
            testImg.fill(Qt::blue);
            SpriteStudio::LicenseManager::applyWatermark(testImg);
            QCOMPARE(testImg.text(QStringLiteral("Generator")), QStringLiteral("SpriteStudio %1 Community Edition").arg(toolStr));
            QCOMPARE(testImg.text(QStringLiteral("X-SpriteStudio-Tool")), toolStr);
            QCOMPARE(testImg.text(QStringLiteral("X-SpriteStudio-License")), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));
            QVERIFY(testImg.text(QStringLiteral("X-SpriteStudio-Notice")).contains(QStringLiteral("<1M$")));

            // 4. JSON metadata watermarking
            QJsonObject jsonMeta;
            jsonMeta["version"] = "1.0";
            SpriteStudio::LicenseManager::applyWatermark(jsonMeta);
            QCOMPARE(jsonMeta["app"].toString(), QStringLiteral("SpriteStudio %1 Community Edition").arg(toolStr));
            QCOMPARE(jsonMeta["tool"].toString(), toolStr);
            QCOMPARE(jsonMeta["license"].toString(), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));

            // 5. Header comment
            QString header = SpriteStudio::LicenseManager::watermarkHeaderComment();
            QVERIFY(header.contains(QStringLiteral("Community Edition")));
            if (t == SpriteStudio::ToolType::CLI) {
                QVERIFY(header.contains(QStringLiteral("Commercial CLI Automation")));
            } else {
                QVERIFY(header.contains(QStringLiteral("Commercial seat license")));
            }
        }
    }
}

void TestCore::testIntegrityGuardAndForensicWatermarking()
{
    bool isComm = SpriteStudio::LicenseManager::isCommercial();

    // 1. Initial integrity verification
    QCOMPARE(SpriteStudio::IntegrityGuard::isCommercialAuthentic(), isComm);
    QVERIFY(!SpriteStudio::IntegrityGuard::isTampered());

    // 2. Test steganographic Alpha == 0 pixel watermarking for GUI and CLI
    for (SpriteStudio::ToolType t : {SpriteStudio::ToolType::GUI, SpriteStudio::ToolType::CLI}) {
        SpriteStudio::LicenseManager::setToolType(t);

        QImage testImg(16, 16, QImage::Format_ARGB32);
        testImg.fill(Qt::transparent); // initially 0x00000000
        for (int y = 6; y < 10; ++y) {
            for (int x = 6; x < 10; ++x) {
                testImg.setPixelColor(x, y, QColor(255, 0, 0, 255));
            }
        }

        SpriteStudio::IntegrityGuard::applySteganographicWatermark(testImg);

        if (isComm) {
            // Commercial: transparent pixels remain clean 0x00000000
            QRgb pTrans = testImg.pixel(0, 0);
            QCOMPARE(qAlpha(pTrans), 0);
            QCOMPARE(pTrans, SpriteStudio::IntegrityGuard::CLEAN_COMMERCIAL_ALPHA0);
        } else {
            // Community: transparent pixels have Alpha == 0 but RGB channels contain:
            // GUI: 'S','S','G' (0x00535347)
            // CLI: 'S','S','C' (0x00535343)
            QRgb expectedMark = (t == SpriteStudio::ToolType::CLI)
                ? SpriteStudio::IntegrityGuard::MAGIC_COMMUNITY_CLI_ALPHA0
                : SpriteStudio::IntegrityGuard::MAGIC_COMMUNITY_GUI_ALPHA0;

            QRgb pTrans = testImg.pixel(0, 0);
            QCOMPARE(qAlpha(pTrans), 0);
            QCOMPARE(pTrans, expectedMark);

            // Center pixel remains opaque red untouched
            QRgb pCenter = testImg.pixel(8, 8);
            QCOMPARE(qAlpha(pCenter), 255);
            QCOMPARE(qRed(pCenter), 255);

            // Test saving to PNG and reloading
            QString tmpPng = QDir::temp().filePath(QStringLiteral("test_stego_%1.png").arg(t == SpriteStudio::ToolType::CLI ? "cli" : "gui"));
            QVERIFY(testImg.save(tmpPng, "PNG"));
            QImage reloaded(tmpPng);
            QVERIFY(!reloaded.isNull());
            QCOMPARE(reloaded.pixel(0, 0), expectedMark);
            QFile::remove(tmpPng);
        }
    }

    // 3. Test Layout Signatures
    QString payload = QStringLiteral("atlas.png:512x512:16");
    QString sig = SpriteStudio::IntegrityGuard::computeLayoutSignature(payload);
    QVERIFY(!sig.isEmpty());
    if (isComm) {
        QVERIFY(sig.startsWith(QStringLiteral("comm-")));
    } else {
        QCOMPARE(sig, QStringLiteral("community-unverified"));
    }
    QVERIFY(SpriteStudio::IntegrityGuard::verifyLayoutSignature(payload, sig));

    // 4. Test Simulated Tamper Detection for GUI and CLI
    SpriteStudio::IntegrityGuard::setSimulatedTampered(true);
    QVERIFY(SpriteStudio::IntegrityGuard::isTampered());
    QVERIFY(!SpriteStudio::IntegrityGuard::isCommercialAuthentic());

    for (SpriteStudio::ToolType t : {SpriteStudio::ToolType::GUI, SpriteStudio::ToolType::CLI}) {
        SpriteStudio::LicenseManager::setToolType(t);

        QRgb expectedTamperMark = (t == SpriteStudio::ToolType::CLI)
            ? SpriteStudio::IntegrityGuard::MAGIC_TAMPERED_CLI_ALPHA0
            : SpriteStudio::IntegrityGuard::MAGIC_TAMPERED_GUI_ALPHA0;

        QImage tamperedImg(10, 10, QImage::Format_ARGB32);
        tamperedImg.fill(Qt::transparent);
        SpriteStudio::IntegrityGuard::applySteganographicWatermark(tamperedImg);
        QCOMPARE(tamperedImg.pixel(0, 0), expectedTamperMark);

        // Metadata under tampered mode
        QJsonObject tamperedMeta;
        SpriteStudio::LicenseManager::applyWatermark(tamperedMeta);
        QCOMPARE(tamperedMeta.value(QStringLiteral("integrity")).toString(),
                 QStringLiteral("TAMPERED_CIRCUMVENTION_DETECTED"));
        QCOMPARE(tamperedMeta.value(QStringLiteral("signature")).toString(),
                 QStringLiteral("tampered-tamper-detected"));

        // Header comment under tampered mode
        QString tamperedHead = SpriteStudio::LicenseManager::watermarkHeaderComment();
        QVERIFY(tamperedHead.contains(QStringLiteral("Tampered")));
    }

    // Reset simulated tamper state and default tool type
    SpriteStudio::IntegrityGuard::setSimulatedTampered(false);
    SpriteStudio::LicenseManager::setToolType(SpriteStudio::ToolType::GUI);
    QVERIFY(!SpriteStudio::IntegrityGuard::isTampered());
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestCore tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_core.moc"
