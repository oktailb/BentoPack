#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "config/appconfig.h"

class TestAppConfig : public QObject
{
    Q_OBJECT

private slots:
    void testAppConfigDefaults();
    void testAppConfigSaveAndLoad();
    void testAppConfigCorruptJsonFallback();
};

void TestAppConfig::testAppConfigDefaults()
{
    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // General defaults
    QCOMPARE(cfg.general().language, QStringLiteral("system"));

    // Atlas defaults
    QCOMPARE(cfg.atlas().zoomMin, 0.1);
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.atlas().zoomStep, 1.15);
    QCOMPARE(cfg.atlas().minSliceSize, 3);
    QCOMPARE(cfg.atlas().defaultAlphaThreshold, 1);
    QCOMPARE(cfg.atlas().defaultVerticalTolerance, 0);
    QCOMPARE(cfg.atlas().nudgeStepSmall, 1);
    QCOMPARE(cfg.atlas().nudgeStepLarge, 10);
    QCOMPARE(cfg.atlas().fitViewPadding, 20);
    QCOMPARE(cfg.atlas().showPolygonMeshByDefault, true);

    // Visuals defaults
    QCOMPARE(cfg.visuals().handleSize, 8.0);
    QCOMPARE(cfg.visuals().handleMargin, 16.0);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 200, 0));
    QCOMPARE(cfg.visuals().meshWireframeColor, QColor(0, 220, 255, 120));

    // Animation defaults
    QCOMPARE(cfg.animation().defaultFps, 12);
    QCOMPARE(cfg.animation().minFps, 1);
    QCOMPARE(cfg.animation().maxFps, 60);
    QCOMPARE(cfg.animation().autoPlayOnSelection, true);

    // Project defaults
    QCOMPARE(cfg.project().maxRecentFiles, 10);
    QCOMPARE(cfg.project().backgroundRemovalTolerance, 10);
    QCOMPARE(cfg.project().backgroundMinAlpha, 10);
    QCOMPARE(cfg.project().autoSaveSession, true);

    // Export defaults
    QCOMPARE(cfg.exportSettings().defaultPadding, 2);
    QCOMPARE(cfg.exportSettings().defaultBorderPadding, 0);
    QCOMPARE(cfg.exportSettings().defaultExtrude, 0);
    QCOMPARE(cfg.exportSettings().defaultMaxWidth, 4096);
    QCOMPARE(cfg.exportSettings().defaultPowerOfTwo, false);
    QCOMPARE(cfg.exportSettings().defaultTrimSprites, false);

    // Retro Linter defaults (activated by default)
    QCOMPARE(cfg.retroLinter().enabled, true);
    QCOMPARE(cfg.retroLinter().profileIndex, 0);
    QCOMPARE(cfg.retroLinter().scanlineSpriteWidthLimit, 64);
    QCOMPARE(cfg.retroLinter().maxVramTiles, 256);
    QCOMPARE(cfg.retroLinter().tileSize, 8);
    QCOMPARE(cfg.retroLinter().guideLineThickness, 2);
    QCOMPARE(cfg.retroLinter().maxSpritesPerScanline, 8);
    QCOMPARE(cfg.retroLinter().maxColorsPerSprite, 16);
    QCOMPARE(cfg.retroLinter().showScanlineGuide, true);
    QCOMPARE(cfg.retroLinter().showTileGrid, true);
    QCOMPARE(cfg.retroLinter().showTimelineBadges, true);
    QCOMPARE(cfg.retroLinter().showVramGauge, true);
    QCOMPARE(cfg.retroLinter().showAtlasGuides, true);
}

void TestAppConfig::testAppConfigSaveAndLoad()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString tempConfigPath = tempDir.filePath(QStringLiteral("test_config.json"));

    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // Modify some values
    cfg.general().language = QStringLiteral("ja_JA");
    cfg.atlas().zoomMax = 20.0;
    cfg.atlas().minSliceSize = 5;
    cfg.atlas().nudgeStepSmall = 2;
    cfg.atlas().nudgeStepLarge = 16;
    cfg.atlas().showPolygonMeshByDefault = false;
    cfg.animation().defaultFps = 24;
    cfg.animation().autoPlayOnSelection = false;
    cfg.project().maxRecentFiles = 15;
    cfg.project().autoSaveSession = false;
    cfg.project().backgroundMinAlpha = 25;
    cfg.visuals().selectedBoxColor = QColor(255, 0, 0);
    cfg.exportSettings().defaultPadding = 4;
    cfg.exportSettings().defaultExtrude = 1;
    cfg.exportSettings().defaultPowerOfTwo = true;
    cfg.exportSettings().defaultTrimSprites = true;
    cfg.retroLinter().profileIndex = 4; // Mega Drive
    cfg.retroLinter().scanlineSpriteWidthLimit = 128;
    cfg.retroLinter().maxVramTiles = 512;
    cfg.retroLinter().guideLineThickness = 3;
    cfg.retroLinter().maxSpritesPerScanline = 16;
    cfg.retroLinter().maxColorsPerSprite = 16;
    cfg.retroLinter().showScanlineGuide = false;
    cfg.retroLinter().showAtlasGuides = false;

    // Save to temp path
    bool saveOk = cfg.save(tempConfigPath);
    QVERIFY(saveOk);
    QVERIFY(QFile::exists(tempConfigPath));

    // Reset to defaults
    cfg.resetToDefaults();
    QCOMPARE(cfg.general().language, QStringLiteral("system"));
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.atlas().nudgeStepSmall, 1);
    QCOMPARE(cfg.animation().defaultFps, 12);
    QCOMPARE(cfg.animation().autoPlayOnSelection, true);
    QCOMPARE(cfg.project().autoSaveSession, true);
    QCOMPARE(cfg.exportSettings().defaultPadding, 2);
    QCOMPARE(cfg.retroLinter().scanlineSpriteWidthLimit, 64);
    QCOMPARE(cfg.retroLinter().guideLineThickness, 2);
    QCOMPARE(cfg.retroLinter().showAtlasGuides, true);

    // Load back from temp path
    bool loadOk = cfg.load(tempConfigPath);
    QVERIFY(loadOk);

    // Verify modified values are recovered
    QCOMPARE(cfg.general().language, QStringLiteral("ja_JA"));
    QCOMPARE(cfg.atlas().zoomMax, 20.0);
    QCOMPARE(cfg.atlas().minSliceSize, 5);
    QCOMPARE(cfg.atlas().nudgeStepSmall, 2);
    QCOMPARE(cfg.atlas().nudgeStepLarge, 16);
    QCOMPARE(cfg.atlas().showPolygonMeshByDefault, false);
    QCOMPARE(cfg.animation().defaultFps, 24);
    QCOMPARE(cfg.animation().autoPlayOnSelection, false);
    QCOMPARE(cfg.project().maxRecentFiles, 15);
    QCOMPARE(cfg.project().autoSaveSession, false);
    QCOMPARE(cfg.project().backgroundMinAlpha, 25);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 0, 0));
    QCOMPARE(cfg.exportSettings().defaultPadding, 4);
    QCOMPARE(cfg.exportSettings().defaultExtrude, 1);
    QCOMPARE(cfg.exportSettings().defaultPowerOfTwo, true);
    QCOMPARE(cfg.exportSettings().defaultTrimSprites, true);
    QCOMPARE(cfg.retroLinter().profileIndex, 4);
    QCOMPARE(cfg.retroLinter().scanlineSpriteWidthLimit, 128);
    QCOMPARE(cfg.retroLinter().maxVramTiles, 512);
    QCOMPARE(cfg.retroLinter().guideLineThickness, 3);
    QCOMPARE(cfg.retroLinter().maxSpritesPerScanline, 16);
    QCOMPARE(cfg.retroLinter().maxColorsPerSprite, 16);
    QCOMPARE(cfg.retroLinter().showScanlineGuide, false);
    QCOMPARE(cfg.retroLinter().showAtlasGuides, false);

    // Clean up
    cfg.resetToDefaults();
}

void TestAppConfig::testAppConfigCorruptJsonFallback()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString corruptPath = tempDir.filePath(QStringLiteral("corrupt.json"));

    // Write incomplete / invalid JSON
    {
        QFile file(corruptPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("{ \"atlas\": { \"zoom_max\": 999.0, INVALID SYNTAX ... ");
        file.close();
    }

    AppConfig &cfg = AppConfig::instance();
    cfg.resetToDefaults();

    // Loading corrupt JSON must fail gracefully without throwing or crashing
    bool loadOk = cfg.load(corruptPath);
    QVERIFY(!loadOk);

    // Safe defaults must be preserved
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.animation().defaultFps, 12);
}

QTEST_MAIN(TestAppConfig)
#include "test_app_config.moc"
