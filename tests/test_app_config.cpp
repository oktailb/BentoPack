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

    // Visuals defaults
    QCOMPARE(cfg.visuals().handleSize, 8.0);
    QCOMPARE(cfg.visuals().handleMargin, 16.0);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 200, 0));

    // Animation defaults
    QCOMPARE(cfg.animation().defaultFps, 12);
    QCOMPARE(cfg.animation().minFps, 1);
    QCOMPARE(cfg.animation().maxFps, 60);

    // Project defaults
    QCOMPARE(cfg.project().maxRecentFiles, 10);
    QCOMPARE(cfg.project().backgroundRemovalTolerance, 10);
    QCOMPARE(cfg.project().backgroundMinAlpha, 10);
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
    cfg.animation().defaultFps = 24;
    cfg.project().maxRecentFiles = 15;
    cfg.visuals().selectedBoxColor = QColor(255, 0, 0);

    // Save to temp path
    bool saveOk = cfg.save(tempConfigPath);
    QVERIFY(saveOk);
    QVERIFY(QFile::exists(tempConfigPath));

    // Reset to defaults
    cfg.resetToDefaults();
    QCOMPARE(cfg.general().language, QStringLiteral("system"));
    QCOMPARE(cfg.atlas().zoomMax, 10.0);
    QCOMPARE(cfg.animation().defaultFps, 12);

    // Load back from temp path
    bool loadOk = cfg.load(tempConfigPath);
    QVERIFY(loadOk);

    // Verify modified values are recovered
    QCOMPARE(cfg.general().language, QStringLiteral("ja_JA"));
    QCOMPARE(cfg.atlas().zoomMax, 20.0);
    QCOMPARE(cfg.atlas().minSliceSize, 5);
    QCOMPARE(cfg.animation().defaultFps, 24);
    QCOMPARE(cfg.project().maxRecentFiles, 15);
    QCOMPARE(cfg.visuals().selectedBoxColor, QColor(255, 0, 0));

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
