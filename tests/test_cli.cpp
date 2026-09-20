#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include "cli/cliparser.h"
#include "cli/watch_daemon.h"
#include "extractor/extractorregistry.h"
#include "filters/filterregistry.h"

using namespace SpriteStudioCli;

static QString extractUidFromTres(const QString &tresPath)
{
    QFile file(tresPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QString content = QString::fromUtf8(file.read(1024));
    QRegularExpression uidRegex(QStringLiteral("uid=\"([^\"]+)\""));
    QRegularExpressionMatch match = uidRegex.match(content);
    return match.hasMatch() ? match.captured(1) : QString();
}

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testCliHelpAndVersion();
    void testTexturePackerFlavorPacking();
    void testAsepriteFlavorPacking();
    void testGodot4ExportAndUid();
    void testGodotSceneGeneration();
    void testNativeSliceCommand();
    void testNativeFilterCommand();
    void testPosixExitCodes();
    void testJsonOutputMode();
    void testWatchLockAcquisitionAndConflict();
    void testWatchDebouncedRepack();

private:
    QTemporaryDir m_tempDir;
};

void TestCli::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    QString binPlugins = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    ExtractorRegistry::instance().loadPlugins(binPlugins);
    FilterRegistry::instance().loadPlugins(binPlugins);
}

void TestCli::cleanupTestCase()
{
}

void TestCli::testCliHelpAndVersion()
{
    CliParser parser;
    CliResult resHelp = parser.parseAndExecute({ QStringLiteral("spritestudio-cli"), QStringLiteral("--help") });
    QCOMPARE(resHelp.exitCode, ExitSuccess);
    QVERIFY(resHelp.message.contains(QStringLiteral("SpriteStudio CLI")));

    CliResult resVersion = parser.parseAndExecute({ QStringLiteral("spritestudio-cli"), QStringLiteral("-v") });
    QCOMPARE(resVersion.exitCode, ExitSuccess);
    QVERIFY(resVersion.message.contains(QStringLiteral("v1.0.0")));
}

void TestCli::testTexturePackerFlavorPacking()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QVERIFY(QFile::exists(sampleHero));

    QString outSheet = m_tempDir.filePath(QStringLiteral("tp_sheet.png"));
    QString outData = m_tempDir.filePath(QStringLiteral("tp_data.json"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("TexturePacker"),
        QStringLiteral("--sheet"), outSheet,
        QStringLiteral("--data"), outData,
        QStringLiteral("--format"), QStringLiteral("json-array"),
        QStringLiteral("--algorithm"), QStringLiteral("MaxRects"),
        QStringLiteral("--maxrects-heuristics"), QStringLiteral("BestShortSideFit"),
        QStringLiteral("--size-constraints"), QStringLiteral("POT"),
        QStringLiteral("--padding"), QStringLiteral("4"),
        QStringLiteral("--extrude"), QStringLiteral("1"),
        QStringLiteral("--trim-mode"), QStringLiteral("Trim"),
        sampleHero
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(outSheet));
    QVERIFY(QFile::exists(outData));

    // Verify JSON format
    QFile jsonFile(outData);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();

    QVERIFY(doc.isObject());
    QJsonObject root = doc.object();
    QVERIFY(root.contains(QStringLiteral("frames")));
    QVERIFY(root[QStringLiteral("frames")].isArray());
    QJsonArray frames = root[QStringLiteral("frames")].toArray();
    QVERIFY(frames.size() > 0);

    QVERIFY(root.contains(QStringLiteral("meta")));
    QJsonObject meta = root[QStringLiteral("meta")].toObject();
    QCOMPARE(meta[QStringLiteral("image")].toString(), QStringLiteral("tp_sheet.png"));

    // Verify POT dimensions
    QImage atlas(outSheet);
    QVERIFY(!atlas.isNull());
    int w = atlas.width();
    int h = atlas.height();
    QVERIFY((w & (w - 1)) == 0); // Is power of 2
    QVERIFY((h & (h - 1)) == 0);
}

void TestCli::testAsepriteFlavorPacking()
{
    QString sampleJson = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.json");
    QVERIFY(QFile::exists(sampleJson));

    QString outSheet = m_tempDir.filePath(QStringLiteral("ase_sheet.png"));
    QString outData = m_tempDir.filePath(QStringLiteral("ase_data.json"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("aseprite"),
        QStringLiteral("-b"),
        sampleJson,
        QStringLiteral("--sheet"), outSheet,
        QStringLiteral("--data"), outData,
        QStringLiteral("--format"), QStringLiteral("json-array"),
        QStringLiteral("--sheet-type"), QStringLiteral("packed"),
        QStringLiteral("--inner-padding"), QStringLiteral("2"),
        QStringLiteral("--list-tags")
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(outSheet));
    QVERIFY(QFile::exists(outData));

    QFile jsonFile(outData);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();

    QVERIFY(doc.isObject());
    QJsonObject root = doc.object();
    QVERIFY(root.contains(QStringLiteral("meta")));
    QJsonObject meta = root[QStringLiteral("meta")].toObject();
    QVERIFY(meta.contains(QStringLiteral("frameTags")));
    QJsonArray tags = meta[QStringLiteral("frameTags")].toArray();
    QVERIFY(tags.size() > 0);
}

void TestCli::testGodot4ExportAndUid()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString outSheet = m_tempDir.filePath(QStringLiteral("godot_sheet.png"));
    QString outTres = m_tempDir.filePath(QStringLiteral("godot_data.tres"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("pack"),
        QStringLiteral("--format"), QStringLiteral("godot4"),
        QStringLiteral("--sheet"), outSheet,
        QStringLiteral("--data"), outTres,
        sampleHero
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(outSheet));
    QVERIFY(QFile::exists(outTres));

    // Verify .tres content
    QFile file(outTres);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QVERIFY(content.contains(QStringLiteral("[gd_resource type=\"SpriteFrames\"")));
    QVERIFY(content.contains(QStringLiteral("uid=\"uid://")));
    QVERIFY(content.contains(QStringLiteral("[sub_resource type=\"AtlasTexture\"")));

    QString originalUid = extractUidFromTres(outTres);
    QVERIFY(!originalUid.isEmpty());

    // Re-run export and verify UID preservation
    CliResult res2 = parser.parseAndExecute(args);
    QCOMPARE(res2.exitCode, ExitSuccess);
    QString preservedUid = extractUidFromTres(outTres);
    QCOMPARE(preservedUid, originalUid);
}

void TestCli::testGodotSceneGeneration()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString outSheet = m_tempDir.filePath(QStringLiteral("godot_scene_sheet.png"));
    QString outTres = m_tempDir.filePath(QStringLiteral("godot_scene_data.tres"));
    QString outTscn = m_tempDir.filePath(QStringLiteral("player.tscn"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("pack"),
        QStringLiteral("--format"), QStringLiteral("godot4"),
        QStringLiteral("--sheet"), outSheet,
        QStringLiteral("--data"), outTres,
        QStringLiteral("--godot-scene"), outTscn,
        sampleHero
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(outTscn));

    QFile file(outTscn);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QVERIFY(content.contains(QStringLiteral("[node name=\"Player\" type=\"AnimatedSprite2D\"]")));
    QVERIFY(content.contains(QStringLiteral("sprite_frames = ExtResource")));
}

void TestCli::testNativeSliceCommand()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString sliceDir = m_tempDir.filePath(QStringLiteral("slices_out"));
    QString projectOut = m_tempDir.filePath(QStringLiteral("sliced_project.ssp"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("slice"),
        QStringLiteral("--smart-crop"),
        QStringLiteral("--output-dir"), sliceDir,
        QStringLiteral("--output-project"), projectOut,
        sampleHero
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QDir(sliceDir).exists());
    QVERIFY(QFile::exists(projectOut));

    QDir dir(sliceDir);
    QStringList slices = dir.entryList({ QStringLiteral("*.png") }, QDir::Files);
    QVERIFY(slices.size() > 0);
}

void TestCli::testNativeFilterCommand()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString outFiltered = m_tempDir.filePath(QStringLiteral("hero_outlined.png"));

    CliParser parser;
    QStringList args = {
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("filter"),
        QStringLiteral("--outline"), QStringLiteral("2"),
        QStringLiteral("--outline-color"), QStringLiteral("#FF0000"),
        QStringLiteral("--output"), outFiltered,
        sampleHero
    };

    CliResult res = parser.parseAndExecute(args);
    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(QFile::exists(outFiltered));

    QImage filtered(outFiltered);
    QVERIFY(!filtered.isNull());
}

void TestCli::testPosixExitCodes()
{
    CliParser parser;

    // 1. Unknown command / syntax error -> ExitSyntaxError (1)
    CliResult resSyntax = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("unknown_cmd_xyz")
    });
    QCOMPARE(resSyntax.exitCode, ExitSyntaxError);

    // 2. File not found -> ExitFileNotFound (2)
    CliResult resNotFound = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("--sheet"), QStringLiteral("out.png"),
        QStringLiteral("does_not_exist_987654.png")
    });
    QCOMPARE(resNotFound.exitCode, ExitFileNotFound);

    // 3. Constraint failed (max-size exceeded) -> ExitConstraintFailed (3)
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    CliResult resConstraint = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("--sheet"), m_tempDir.filePath(QStringLiteral("fail.png")),
        QStringLiteral("--max-size"), QStringLiteral("4"), QStringLiteral("4"),
        sampleHero
    });
    QCOMPARE(resConstraint.exitCode, ExitConstraintFailed);
}

void TestCli::testJsonOutputMode()
{
    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString outSheet = m_tempDir.filePath(QStringLiteral("json_mode.png"));

    CliParser parser;
    CliResult res = parser.parseAndExecute({
        QStringLiteral("spritestudio-cli"),
        QStringLiteral("--json"),
        QStringLiteral("--sheet"), outSheet,
        sampleHero
    });

    QCOMPARE(res.exitCode, ExitSuccess);
    QVERIFY(parser.isJsonOutput());
    QVERIFY(res.json.contains(QStringLiteral("status")));
    QCOMPARE(res.json[QStringLiteral("status")].toString(), QStringLiteral("success"));
    QVERIFY(res.json.contains(QStringLiteral("atlas")));
    QVERIFY(res.json.contains(QStringLiteral("width")));
    QVERIFY(res.json.contains(QStringLiteral("height")));
}

void TestCli::testWatchLockAcquisitionAndConflict()
{
    QString dirA = m_tempDir.filePath(QStringLiteral("project_alpha"));
    QString dirB = m_tempDir.filePath(QStringLiteral("project_beta"));
    QDir().mkpath(dirA);
    QDir().mkpath(dirB);

    WatchDaemon daemon1;
    qint64 pid1 = 0;
    QVERIFY(daemon1.acquireDirectoryLock(dirA, pid1));

    qint64 checkPid = 0;
    QVERIFY(WatchDaemon::isDirectoryLocked(dirA, checkPid));
    QCOMPARE(checkPid, static_cast<qint64>(QCoreApplication::applicationPid()));

    // Attempting concurrent lock on the same directory MUST fail (isolated lock)
    WatchDaemon daemonConflict;
    qint64 conflictPid = 0;
    bool lockedSame = daemonConflict.acquireDirectoryLock(dirA, conflictPid);
    QVERIFY(!lockedSame);
    QCOMPARE(conflictPid, checkPid);

    // Multi-instance: attempting lock on a DIFFERENT directory MUST succeed
    WatchDaemon daemon2;
    qint64 pid2 = 0;
    bool lockedDiff = daemon2.acquireDirectoryLock(dirB, pid2);
    QVERIFY(lockedDiff);
    QVERIFY(WatchDaemon::isDirectoryLocked(dirB, pid2));

    // Release dirA lock and verify it can now be acquired
    daemon1.releaseDirectoryLock();
    QVERIFY(!WatchDaemon::isDirectoryLocked(dirA, checkPid));

    bool nowAcquired = daemonConflict.acquireDirectoryLock(dirA, conflictPid);
    QVERIFY(nowAcquired);

    daemonConflict.releaseDirectoryLock();
    daemon2.releaseDirectoryLock();
}

void TestCli::testWatchDebouncedRepack()
{
    QString watchFolder = m_tempDir.filePath(QStringLiteral("watch_sprites"));
    QDir().mkpath(watchFolder);

    QString sampleHero = QStringLiteral(SAMPLE_DIR) + QStringLiteral("/hero.png");
    QString frame1 = QDir(watchFolder).filePath(QStringLiteral("frame_01.png"));
    QFile::copy(sampleHero, frame1);

    QString outSheet = m_tempDir.filePath(QStringLiteral("watch_atlas.png"));
    QString outData = m_tempDir.filePath(QStringLiteral("watch_atlas.json"));

    int repackCount = 0;
    auto repackCb = [&repackCount, outSheet, outData, watchFolder]() -> CliResult {
        repackCount++;
        CliParser p;
        return p.parseAndExecute({
            QStringLiteral("spritestudio-cli"),
            QStringLiteral("--sheet"), outSheet,
            QStringLiteral("--data"), outData,
            watchFolder
        });
    };

    WatchDaemon daemon;
    CliResult startRes = daemon.startWatching({ watchFolder }, outSheet, outData, repackCb, 50);
    QCOMPARE(startRes.exitCode, ExitSuccess);

    // Initial pack executed on start
    QCOMPARE(repackCount, 1);
    QVERIFY(QFile::exists(outSheet));
    QVERIFY(QFile::exists(outData));

    // Simulate file addition in watched directory
    QString frame2 = QDir(watchFolder).filePath(QStringLiteral("frame_02.png"));
    QFile::copy(sampleHero, frame2);

    // Wait for QFileSystemWatcher and debounce timer (50ms debounce)
    QTest::qWait(200);

    QVERIFY(repackCount >= 2);
    daemon.releaseDirectoryLock();
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestCli tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_cli.moc"
