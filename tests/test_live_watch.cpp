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

#include <QTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QSettings>
#include "watcher/livewatchmanager.h"

class TestLiveWatch : public QObject
{
    Q_OBJECT

private slots:
    void testConfigDefaultsAndComputedPath();
    void testConfigSettingsRoundtrip();
    void testFileReadyVerification();
    void testWatchLifecycleAndManualReload();
};

void TestLiveWatch::testConfigDefaultsAndComputedPath()
{
    const QString mockPath = QStringLiteral("/path/to/game/hero.ase");
    LiveWatchConfig cfg = LiveWatchConfig::createDefault(mockPath);
    QCOMPARE(cfg.sourcePath, mockPath);
    // On Unix this is "/path/to/game"; on Windows QFileInfo may resolve drive-relative paths with drive letter (e.g. "D:/path/to/game")
    QVERIFY(cfg.outputDirectory == QStringLiteral("/path/to/game") ||
            cfg.outputDirectory == QFileInfo(mockPath).path() ||
            cfg.outputDirectory == QFileInfo(mockPath).absolutePath());
    QCOMPARE(cfg.outputBaseName, QStringLiteral("hero"));
    QCOMPARE(cfg.engineFormatId, QStringLiteral("godot"));
    QVERIFY(cfg.autoExport);
    QCOMPARE(cfg.debounceMs, 250);
    QVERIFY(cfg.exportOptions.packOptions.enableCelDeduplication);

    // Default Godot format computes .tres
    QString godotPath = cfg.computedExportPath();
    QVERIFY(godotPath.endsWith(QStringLiteral("hero.tres")));

    // Unity format computes .prefab
    cfg.engineFormatId = QStringLiteral("unity");
    QString unityPath = cfg.computedExportPath();
    QVERIFY(unityPath.endsWith(QStringLiteral("hero.prefab")));

    // Unreal format computes .paper2d.json
    cfg.engineFormatId = QStringLiteral("unreal");
    QString unrealPath = cfg.computedExportPath();
    QVERIFY(unrealPath.endsWith(QStringLiteral("hero.paper2d.json")));

    // TexturePacker JSON computes .json
    cfg.engineFormatId = QStringLiteral("json");
    QString jsonPath = cfg.computedExportPath();
    QVERIFY(jsonPath.endsWith(QStringLiteral("hero.json")));
}

void TestLiveWatch::testConfigSettingsRoundtrip()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString iniPath = tempDir.filePath(QStringLiteral("test_config.ini"));
    QSettings settings(iniPath, QSettings::IniFormat);

    LiveWatchConfig original;
    original.sourceType = WatchSourceType::Directory;
    original.sourcePath = QStringLiteral("/my/sprites");
    original.fileFilters = QStringLiteral("*.png;*.ase");
    original.debounceMs = 400;
    original.autoExport = false;
    original.outputDirectory = QStringLiteral("/my/game/assets");
    original.outputBaseName = QStringLiteral("player_sheet");
    original.engineFormatId = QStringLiteral("unity");
    original.exportOptions.textureFormat = TEXTURE_FORMAT_KTX2_UASTC;
    original.exportOptions.packOptions.algorithm = AtlasPacker::TightPolygon;
    original.exportOptions.packOptions.enableCelDeduplication = true;
    original.exportOptions.packOptions.enableDeltaPatching = true;
    original.exportOptions.packOptions.padding = 4;

    original.save(settings);

    LiveWatchConfig loaded;
    loaded.load(settings);

    QCOMPARE(loaded.sourceType, WatchSourceType::Directory);
    QCOMPARE(loaded.sourcePath, QStringLiteral("/my/sprites"));
    QCOMPARE(loaded.fileFilters, QStringLiteral("*.png;*.ase"));
    QCOMPARE(loaded.debounceMs, 400);
    QCOMPARE(loaded.autoExport, false);
    QCOMPARE(loaded.outputDirectory, QStringLiteral("/my/game/assets"));
    QCOMPARE(loaded.outputBaseName, QStringLiteral("player_sheet"));
    QCOMPARE(loaded.engineFormatId, QStringLiteral("unity"));
    QCOMPARE(loaded.exportOptions.textureFormat, TEXTURE_FORMAT_KTX2_UASTC);
    QCOMPARE(loaded.exportOptions.packOptions.algorithm, AtlasPacker::TightPolygon);
    QVERIFY(loaded.exportOptions.packOptions.enableCelDeduplication);
    QVERIFY(loaded.exportOptions.packOptions.enableDeltaPatching);
    QCOMPARE(loaded.exportOptions.packOptions.padding, 4);
}

void TestLiveWatch::testFileReadyVerification()
{
    // Non-existent file
    QVERIFY(!LiveWatchManager::verifyFileReady(QStringLiteral("/non/existent/path/foo.ase")));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Zero-length file
    QString emptyPath = tempDir.filePath(QStringLiteral("empty.ase"));
    {
        QFile f(emptyPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }
    QVERIFY(!LiveWatchManager::verifyFileReady(emptyPath, 2, 10));

    // Valid small PNG file
    QString validPngPath = tempDir.filePath(QStringLiteral("valid.png"));
    {
        QImage img(16, 16, QImage::Format_ARGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(validPngPath, "PNG"));
    }
    QVERIFY(LiveWatchManager::verifyFileReady(validPngPath, 2, 10));

    // Valid dummy Aseprite file with magic bytes 0xA5E0
    QString validAsePath = tempDir.filePath(QStringLiteral("dummy.ase"));
    {
        QFile f(validAsePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QByteArray data(128, 0);
        // Header: offset 0 = uint32 filesize (128)
        *reinterpret_cast<quint32*>(data.data()) = 128;
        // offset 4 = uint16 magic (0xA5E0)
        *reinterpret_cast<quint16*>(data.data() + 4) = 0xA5E0;
        f.write(data);
        f.close();
    }
    QVERIFY(LiveWatchManager::verifyFileReady(validAsePath, 2, 10));
}

void TestLiveWatch::testWatchLifecycleAndManualReload()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString samplePng = tempDir.filePath(QStringLiteral("sprite.png"));
    {
        QImage img(32, 32, QImage::Format_ARGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(samplePng, "PNG"));
    }

    LiveWatchManager manager;
    QCOMPARE(manager.state(), WatchState::Idle);
    QVERIFY(!manager.isWatching());

    LiveWatchConfig cfg = LiveWatchConfig::createDefault(samplePng);
    cfg.autoExport = false; // Disable auto export for fast test
    cfg.debounceMs = 100;

    QString err;
    QVERIFY(manager.startWatching(cfg, &err));
    QVERIFY(manager.isWatching());
    QCOMPARE(manager.state(), WatchState::Watching);

    // Spy on signals
    QSignalSpy spyState(&manager, &LiveWatchManager::stateChanged);
    QSignalSpy spyReload(&manager, &LiveWatchManager::fileReloaded);

    // Trigger manual reload
    manager.triggerManualReload();

    // Wait for async processing
    QVERIFY(spyReload.wait(5000));
    QVERIFY(manager.state() == WatchState::Success || manager.state() == WatchState::Watching);

    manager.stopWatching();
    QVERIFY(!manager.isWatching());
    QCOMPARE(manager.state(), WatchState::Idle);
}

QTEST_MAIN(TestLiveWatch)
#include "test_live_watch.moc"
