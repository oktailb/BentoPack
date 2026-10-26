#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QColor>
#include "image/retrolinterhelper.h"
#include "config/appconfig.h"

class TestRetroLinter : public QObject
{
    Q_OBJECT

private slots:
    void testCountUniqueTilesEmpty();
    void testCountUniqueTilesDeduplication();
    void testScanlineViolationDetection();
    void testPresetProfiles();
    void testViolationMessageContent();
};

void TestRetroLinter::testCountUniqueTilesEmpty()
{
    QList<QImage> emptyList;
    int count = RetroLinterHelper::countUniqueTiles(emptyList, 8);
    QCOMPARE(count, 0);

    // Completely transparent 32x32 image
    QImage transparentImg(32, 32, QImage::Format_ARGB32_Premultiplied);
    transparentImg.fill(Qt::transparent);
    QList<QImage> transparentList;
    transparentList << transparentImg;
    QCOMPARE(RetroLinterHelper::countUniqueTiles(transparentList, 8), 0);
}

void TestRetroLinter::testCountUniqueTilesDeduplication()
{
    // Create an image with two 8x8 tiles that are identical
    // and one tile that is different
    QImage img(24, 8, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    // Tile 0 (0..7): Red dot in center
    img.setPixelColor(3, 3, Qt::red);

    // Tile 1 (8..15): Red dot in center (identical to Tile 0)
    img.setPixelColor(11, 3, Qt::red);

    // Tile 2 (16..23): Blue dot in center (different)
    img.setPixelColor(19, 3, Qt::blue);

    QList<QImage> frames;
    frames << img;

    int uniqueTiles = RetroLinterHelper::countUniqueTiles(frames, 8);
    // Should detect exactly 2 unique non-empty tiles (Red tile, Blue tile)
    QCOMPARE(uniqueTiles, 2);

    // Add a second frame that reuses the exact same Red tile and a new Green tile
    QImage img2(16, 8, QImage::Format_ARGB32_Premultiplied);
    img2.fill(Qt::transparent);
    img2.setPixelColor(3, 3, Qt::red);   // Duplicate
    img2.setPixelColor(11, 3, Qt::green); // New unique tile
    frames << img2;

    int totalUnique = RetroLinterHelper::countUniqueTiles(frames, 8);
    // Total: Red, Blue, Green = 3 unique tiles
    QCOMPARE(totalUnique, 3);
}

void TestRetroLinter::testScanlineViolationDetection()
{
    RetroLinterConfig cfg;
    cfg.enabled = true;
    cfg.scanlineSpriteWidthLimit = 64; // e.g. Game Boy or NES limit

    // Frame <= 64px width should NOT violate
    QImage safeImg(64, 32, QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(RetroLinterHelper::isScanlineViolated(safeImg, cfg), false);

    QImage smallImg(16, 16, QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(RetroLinterHelper::isScanlineViolated(smallImg, cfg), false);

    // Frame > 64px width MUST trigger violation
    QImage wideImg(65, 32, QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(RetroLinterHelper::isScanlineViolated(wideImg, cfg), true);

    QImage largeImg(96, 64, QImage::Format_ARGB32_Premultiplied);
    QCOMPARE(RetroLinterHelper::isScanlineViolated(largeImg, cfg), true);

    // When disabled, no violation is reported
    cfg.enabled = false;
    QCOMPARE(RetroLinterHelper::isScanlineViolated(largeImg, cfg), false);
}

void TestRetroLinter::testPresetProfiles()
{
    RetroLinterConfig cfg;

    // Preset 1: Game Boy
    RetroLinterConfig::applyPreset(1, cfg);
    QCOMPARE(cfg.profileIndex, 1);
    QCOMPARE(cfg.scanlineSpriteWidthLimit, 64);
    QCOMPARE(cfg.maxVramTiles, 256);
    QCOMPARE(cfg.tileSize, 8);

    // Preset 2: NES
    RetroLinterConfig::applyPreset(2, cfg);
    QCOMPARE(cfg.profileIndex, 2);
    QCOMPARE(cfg.scanlineSpriteWidthLimit, 64);
    QCOMPARE(cfg.maxVramTiles, 256);

    // Preset 4: Mega Drive
    RetroLinterConfig::applyPreset(4, cfg);
    QCOMPARE(cfg.profileIndex, 4);
    QCOMPARE(cfg.scanlineSpriteWidthLimit, 128);
    QCOMPARE(cfg.maxVramTiles, 512);

    // Preset 6: SNES
    RetroLinterConfig::applyPreset(6, cfg);
    QCOMPARE(cfg.profileIndex, 6);
    QCOMPARE(cfg.scanlineSpriteWidthLimit, 256);
    QCOMPARE(cfg.maxVramTiles, 512);
}

void TestRetroLinter::testViolationMessageContent()
{
    RetroLinterConfig cfg;
    cfg.enabled = true;
    cfg.scanlineSpriteWidthLimit = 64;

    QImage safeImg(32, 32, QImage::Format_ARGB32_Premultiplied);
    QVERIFY(RetroLinterHelper::scanlineViolationMessage(safeImg, cfg).isEmpty());

    QImage overflowImg(80, 32, QImage::Format_ARGB32_Premultiplied);
    QString msg = RetroLinterHelper::scanlineViolationMessage(overflowImg, cfg);
    QVERIFY(!msg.isEmpty());
    QVERIFY(msg.contains(QStringLiteral("80 px")));
    QVERIFY(msg.contains(QStringLiteral("64 px")));
}

QTEST_MAIN(TestRetroLinter)
#include "test_retro_linter.moc"
