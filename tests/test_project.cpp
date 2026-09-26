#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QPainter>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageWriter>
#include <QUndoStack>

#include "project/sessionmanager.h"
#include "project/projectmanager.h"
#include "controller/projectcontroller.h"
#include "model/spritedocument.h"
#include "commands/commands.h"
#include "include/widgets/githistorydock.h"
#include "include/widgets/settingsdialog.h"
#include "config/appconfig.h"

class TestProject : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSessionManagerStartAndLock();
    void testZipPackingAndUnpacking();
    void testAtomicSaveSessionToSsp();
    void testBentoPackSaveAndLoadRoundtrip();
    void testCrashDetectionAndDiscard();
    void testProjectSerializationFidelity();
    void testProjectManagerSessionDir();
    void testProjectControllerWorkflow();
    void testRecentProjects();
    void testGitIntegration();
    void testGitContinuousSnapshots();
    void testGitSavedInSsp();
    void testGitTimeTravelCheckout();
    void testGitHistoryDockUI();
    void testGitConfigSettingsPersistence();
    void testGitAuthorIdentityInCommits();
    void testSettingsDialogSearchAndApply();

private:
    QTemporaryDir m_tempDir;
};

void TestProject::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestProject::cleanupTestCase()
{
}

void TestProject::testSessionManagerStartAndLock()
{
    SessionManager session;
    QVERIFY(!session.hasActiveSession());

    QString origFile = m_tempDir.filePath("test_orig.bento");
    QVERIFY(session.startNewSession(origFile));
    QVERIFY(session.hasActiveSession());
    QVERIFY(!session.currentSessionDir().isEmpty());
    QVERIFY(QDir(session.currentSessionDir()).exists());
    QVERIFY(QDir(session.sessionAssetsDir()).exists());
    QCOMPARE(session.currentOriginalFilePath(), origFile);

    // Verify lock file
    SessionLockInfo lock = SessionManager::readSessionLock(session.currentSessionDir());
    QCOMPARE(lock.pid, QCoreApplication::applicationPid());
    QCOMPARE(lock.status, QStringLiteral("active"));
    QCOMPARE(lock.originalFilePath, origFile);
    QVERIFY(lock.created.isValid());

    // Test process liveness helper
    QVERIFY(SessionManager::isProcessAlive(QCoreApplication::applicationPid()));
    QVERIFY(!SessionManager::isProcessAlive(99999999));

    session.closeCurrentSession(true);
    QVERIFY(!session.hasActiveSession());
}

void TestProject::testZipPackingAndUnpacking()
{
    QString srcDir = m_tempDir.filePath("zip_src");
    QString dstDir = m_tempDir.filePath("zip_dst");
    QString zipPath = m_tempDir.filePath("test_archive.zip");

    QDir().mkpath(srcDir + "/sub");

    QFile file1(srcDir + "/file1.txt");
    QVERIFY(file1.open(QIODevice::WriteOnly));
    file1.write("Hello BentoPack");
    file1.close();

    QFile file2(srcDir + "/sub/file2.txt");
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("Subdirectory Content");
    file2.close();

    QString errorMsg;
    QVERIFY2(SessionManager::packZip(srcDir, zipPath, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(zipPath));

    QVERIFY2(SessionManager::unpackZip(zipPath, dstDir, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(dstDir + "/file1.txt"));
    QVERIFY(QFile::exists(dstDir + "/sub/file2.txt"));

    QFile read1(dstDir + "/file1.txt");
    QVERIFY(read1.open(QIODevice::ReadOnly));
    QCOMPARE(read1.readAll(), QByteArray("Hello BentoPack"));
    read1.close();

    QFile read2(dstDir + "/sub/file2.txt");
    QVERIFY(read2.open(QIODevice::ReadOnly));
    QCOMPARE(read2.readAll(), QByteArray("Subdirectory Content"));
    read2.close();
}

void TestProject::testAtomicSaveSessionToSsp()
{
    SessionManager session;
    QVERIFY(session.startNewSession());

    // Put a dummy file into session workspace
    QFile pj(session.sessionProjectJsonPath());
    QVERIFY(pj.open(QIODevice::WriteOnly));
    pj.write("{\"format\":\"BentoPackProject\"}");
    pj.close();

    QString targetSsp = m_tempDir.filePath("saved_project.bento");
    QString errorMsg;
    QVERIFY2(session.saveSessionToSsp(targetSsp, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(targetSsp));
    QCOMPARE(session.currentOriginalFilePath(), targetSsp);

    // Save again to test atomic overwrite
    QVERIFY2(session.saveSessionToSsp(targetSsp, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(targetSsp));
    QVERIFY(!QFile::exists(targetSsp + ".tmp")); // Temporary file should be cleanly renamed

    session.closeCurrentSession(true);
}

void TestProject::testBentoPackSaveAndLoadRoundtrip()
{
    SessionManager session;
    QVERIFY(session.startNewSession());

    QFile pj(session.sessionProjectJsonPath());
    QVERIFY(pj.open(QIODevice::WriteOnly));
    pj.write("{\"format\":\"BentoPackProject\",\"generator\":\"BentoPack\"}");
    pj.close();

    QString targetBento = m_tempDir.filePath("saved_project.bento");
    QString errorMsg;
    QVERIFY2(session.saveSessionToBento(targetBento, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(targetBento));
    QCOMPARE(session.currentOriginalFilePath(), targetBento);

    session.closeCurrentSession(true);

    // Now open from .bento
    SessionManager session2;
    QVERIFY2(session2.openSessionFromBento(targetBento, &errorMsg), qPrintable(errorMsg));
    QVERIFY(QFile::exists(session2.sessionProjectJsonPath()));

    QFile readPj(session2.sessionProjectJsonPath());
    QVERIFY(readPj.open(QIODevice::ReadOnly));
    QByteArray content = readPj.readAll();
    readPj.close();
    QVERIFY(content.contains("BentoPackProject"));

    session2.closeCurrentSession(true);
}

void TestProject::testCrashDetectionAndDiscard()
{
    // Artificially create an orphan session folder
    QString root = SessionManager::sessionsRootPath();
    QDir().mkpath(root);
    QString fakeOrphanDir = root + "/fake_crashed_session_uuid";
    QDir().mkpath(fakeOrphanDir);

    // Write a lock file with a dead PID (e.g. 99999999) and "active" status
    QFile lockFile(fakeOrphanDir + "/.session_lock");
    QVERIFY(lockFile.open(QIODevice::WriteOnly));
    QJsonObject obj;
    obj["pid"] = 99999999;
    obj["sessionUuid"] = "fake_crashed_session_uuid";
    obj["originalFilePath"] = "c:/crashed/game_hero.bento";
    obj["status"] = "active";
    obj["created"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    obj["lastModified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    lockFile.write(QJsonDocument(obj).toJson());
    lockFile.close();

    QList<OrphanSessionInfo> orphans = SessionManager::detectOrphanSessions();
    bool found = false;
    for (const auto &info : orphans) {
        if (info.sessionDir == fakeOrphanDir) {
            found = true;
            QCOMPARE(info.projectName, QStringLiteral("game_hero.bento"));
            break;
        }
    }
    QVERIFY(found);

    // Discard the orphan session
    QVERIFY(SessionManager::discardOrphanSession(fakeOrphanDir));
    QVERIFY(!QDir(fakeOrphanDir).exists());

    orphans = SessionManager::detectOrphanSessions();
    for (const auto &info : orphans) {
        QVERIFY(info.sessionDir != fakeOrphanDir);
    }
}

void TestProject::testProjectSerializationFidelity()
{
    SpriteDocument doc;
    doc.setFilePath("test_project.bento");

    // Create 64x64 colored atlas
    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    QPainter painter(&atlas);
    painter.fillRect(0, 0, 16, 16, Qt::red);
    painter.fillRect(16, 0, 16, 16, Qt::green);
    painter.fillRect(0, 16, 32, 32, Qt::blue);
    painter.end();
    doc.setAtlas(atlas);

    // Add slices
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 32, 32));
    QCOMPARE(doc.boxes().size(), 3);

    // Add animations
    doc.setAnimation(QStringLiteral("run"), {0, 1}, 10, true);
    doc.setAnimation(QStringLiteral("jump"), {2}, 8, false);

    // Set pivots: box 0 default, box 1 custom pivot, box 2 preset
    doc.setBoxPivot(1, QPoint(12, 14));
    doc.applyPivotPreset({2}, PivotPreset::TopRight);

    // Serialize
    QByteArray json = ProjectManager::serializeDocumentToJson(doc, QStringLiteral("assets/atlas.png"), 2.0, QPointF(10, 20));
    QVERIFY(!json.isEmpty());

    // Save atlas to temp assets folder so deserialize can reload it
    QString dummyAssetsDir = m_tempDir.filePath("dummy_session");
    QDir().mkpath(dummyAssetsDir + "/assets");
    atlas.save(dummyAssetsDir + "/assets/atlas.png", "PNG");

    SpriteDocument restoredDoc;
    double zoom = 1.0;
    QPointF pan;
    QString errorMsg;
    QVERIFY2(ProjectManager::deserializeJsonToDocument(json, restoredDoc, dummyAssetsDir, &zoom, &pan, &errorMsg), qPrintable(errorMsg));

    QCOMPARE(zoom, 2.0);
    QCOMPARE(pan, QPointF(10, 20));
    QCOMPARE(restoredDoc.boxes().size(), 3);
    QCOMPARE(restoredDoc.box(0).rect, QRect(0, 0, 16, 16));
    QCOMPARE(restoredDoc.box(1).rect, QRect(16, 0, 16, 16));
    QCOMPARE(restoredDoc.box(2).rect, QRect(0, 16, 32, 32));

    // Verify pivot roundtrip
    QCOMPARE(restoredDoc.box(0).hasCustomPivot, false);
    QCOMPARE(restoredDoc.box(0).effectivePivot(), QPoint(8, 16));
    QCOMPARE(restoredDoc.box(1).hasCustomPivot, true);
    QCOMPARE(restoredDoc.box(1).pivot, QPoint(12, 14));
    QCOMPARE(restoredDoc.box(2).hasCustomPivot, true);
    QCOMPARE(restoredDoc.box(2).pivot, QPoint(32, 0));

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("run")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("run")).fps, 10);
    QCOMPARE(restoredDoc.animation(QStringLiteral("run")).loop, true);
    QCOMPARE(restoredDoc.animation(QStringLiteral("run")).frameIndices, QList<int>({0, 1}));

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("jump")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("jump")).fps, 8);
    QCOMPARE(restoredDoc.animation(QStringLiteral("jump")).loop, false);
}

void TestProject::testProjectManagerSessionDir()
{
    SpriteDocument doc;
    QImage atlas(32, 32, QImage::Format_ARGB32);
    atlas.fill(Qt::yellow);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.setAnimation(QStringLiteral("idle"), {0}, 6, true);

    QString sessionDir = m_tempDir.filePath("session_dir_test");
    QString errorMsg;
    QVERIFY2(ProjectManager::saveProjectToSessionDir(doc, sessionDir, 1.5, QPointF(5, 5), &errorMsg), qPrintable(errorMsg));

    QVERIFY(QFile::exists(sessionDir + "/project.json"));
    if (QImageWriter::supportedImageFormats().contains("webp")) {
        QVERIFY(QFile::exists(sessionDir + "/assets/atlas.webp"));
    } else {
        QVERIFY(QFile::exists(sessionDir + "/assets/atlas.png"));
    }

    SpriteDocument loadedDoc;
    double zoom = 1.0;
    QPointF pan;
    QVERIFY2(ProjectManager::loadProjectFromSessionDir(sessionDir, loadedDoc, &zoom, &pan, &errorMsg), qPrintable(errorMsg));

    QCOMPARE(zoom, 1.5);
    QCOMPARE(loadedDoc.boxes().size(), 1);
    QCOMPARE(loadedDoc.box(0).rect, QRect(0, 0, 16, 16));
    QVERIFY(loadedDoc.hasAnimation(QStringLiteral("idle")));
}

void TestProject::testProjectControllerWorkflow()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    // Initial state
    QVERIFY(controller.newProject());
    QVERIFY(!controller.isProjectModified());
    QVERIFY(controller.currentProjectPath().isEmpty());

    // Adding content should mark modified
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::cyan);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    QVERIFY(controller.isProjectModified());

    // Save project
    QString sspFile = m_tempDir.filePath("controller_project.bento");
    QString errorMsg;
    QVERIFY2(controller.saveProject(sspFile, &errorMsg), qPrintable(errorMsg));
    QVERIFY(!controller.isProjectModified());
    QCOMPARE(controller.currentProjectPath(), sspFile);
    QVERIFY(QFile::exists(sspFile));

    // Reset and reload
    QVERIFY(controller.newProject());
    QCOMPARE(doc.boxes().size(), 0);

    QVERIFY2(controller.openProject(sspFile, &errorMsg), qPrintable(errorMsg));
    QCOMPARE(doc.boxes().size(), 1);
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 16, 16));
    QVERIFY(!controller.isProjectModified());
}

void TestProject::testRecentProjects()
{
    SpriteDocument doc;
    ProjectController controller(&doc);

    controller.clearRecentProjects();
    QVERIFY(controller.recentProjects().isEmpty());

    QString dummyProj = m_tempDir.filePath("recent_test.bento");
    QFile f(dummyProj);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("test");
    f.close();

    controller.addRecentProject(dummyProj);
    QStringList recents = controller.recentProjects();
    QCOMPARE(recents.size(), 1);
    QCOMPARE(recents.first(), dummyProj);

    controller.clearRecentProjects();
    QVERIFY(controller.recentProjects().isEmpty());
}

void TestProject::testGitIntegration()
{
    SessionManager session;
    QVERIFY(session.startNewSession());

    if (SessionManager::isGitAvailable()) {
        QVERIFY(session.gitInit());
        QVERIFY(session.gitCommit(QStringLiteral("Initial commit in test")));
    } else {
        // If libgit2 is not present, gitInit and isGitAvailable should gracefully return false without crashing
        QVERIFY(!SessionManager::isGitAvailable());
        QVERIFY(!session.gitInit());
    }

    session.closeCurrentSession(true);
}

void TestProject::testGitContinuousSnapshots()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);
    QVERIFY(controller.newProject());

    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::blue);
    doc.setAtlas(atlas);
    ProjectManager::saveProjectToSessionDir(doc, controller.sessionManager()->currentSessionDir());

    // Execute undo commands
    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 16, 16)));
    undoStack.push(new AddSliceCommand(&doc, QRect(16, 0, 16, 16)));
    undoStack.push(new ChangeBoxRectCommand(&doc, 0, QRect(0, 0, 16, 16), QRect(2, 2, 16, 16)));

    // Verify git log has commits for each command
    QList<GitCommitInfo> log = controller.sessionManager()->gitLog();
    for (const auto &c : log) {
        qDebug() << "Commit in log:" << c.message;
    }
    QVERIFY(log.size() >= 3);
    QVERIFY(log[0].message.contains("Resize") || log[0].message.contains("Move") || log[0].message.contains("Slice"));
    QVERIFY(!log[0].hash.isEmpty());
    QVERIFY(log[0].timestamp.isValid());
}

void TestProject::testGitSavedInSsp()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);
    QVERIFY(controller.newProject());

    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::magenta);
    doc.setAtlas(atlas);
    ProjectManager::saveProjectToSessionDir(doc, controller.sessionManager()->currentSessionDir());

    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 32, 32)));

    QString sspPath = m_tempDir.filePath("git_project.bento");
    QVERIFY(controller.saveProject(sspPath));

    // Verify loading the saved project preserves the git log
    SpriteDocument restoredDoc;
    QUndoStack restoredStack;
    ProjectController restoredController(&restoredDoc, &restoredStack);
    QVERIFY(restoredController.openProject(sspPath));

    QList<GitCommitInfo> restoredLog = restoredController.sessionManager()->gitLog();
    QVERIFY(!restoredLog.isEmpty());
    bool foundSaveCommit = false;
    for (const auto &c : restoredLog) {
        if (c.message.contains("Project saved")) {
            foundSaveCommit = true;
            break;
        }
    }
    QVERIFY(foundSaveCommit);
}

void TestProject::testGitTimeTravelCheckout()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);
    QVERIFY(controller.newProject());

    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::cyan);
    doc.setAtlas(atlas);
    ProjectManager::saveProjectToSessionDir(doc, controller.sessionManager()->currentSessionDir());

    // Commit 1: 1 slice
    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 16, 16)));
    QCOMPARE(doc.boxes().size(), 1);

    QList<GitCommitInfo> log1 = controller.sessionManager()->gitLog();
    QVERIFY(!log1.isEmpty());
    QString firstCommitHash = log1.first().hash;

    // Commit 2 & 3: 2 more slices
    undoStack.push(new AddSliceCommand(&doc, QRect(16, 0, 16, 16)));
    undoStack.push(new AddSliceCommand(&doc, QRect(32, 0, 16, 16)));
    QCOMPARE(doc.boxes().size(), 3);

    // Time-travel checkout to first commit
    QString err;
    QVERIFY2(controller.checkoutRevision(firstCommitHash, &err), qPrintable(err));

    // Verify document was restored to exactly 1 slice!
    QCOMPARE(doc.boxes().size(), 1);
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 16, 16));
}

void TestProject::testGitHistoryDockUI()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);
    QVERIFY(controller.newProject());

    QImage atlas(64, 64, QImage::Format_ARGB32);
    atlas.fill(Qt::yellow);
    doc.setAtlas(atlas);
    ProjectManager::saveProjectToSessionDir(doc, controller.sessionManager()->currentSessionDir());

    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 20, 20)));
    undoStack.push(new AddSliceCommand(&doc, QRect(20, 0, 20, 20)));
    QCOMPARE(doc.boxes().size(), 2);

    GitHistoryDock dock;
    dock.setProjectController(&controller);

    QList<GitCommitInfo> log = controller.sessionManager()->gitLog();
    QVERIFY(log.size() >= 2);

    // Test checkout to the oldest action commit through dock
    GitCommitInfo oldestActionCommit = (log.size() >= 2) ? log[log.size() - 2] : log.last();
    QString targetHash = oldestActionCommit.hash;
    dock.checkoutRevision(targetHash);

    // Verify document was updated through dock's checkout
    QCOMPARE(doc.boxes().size(), 1);

    // Test checkout head through dock
    dock.checkoutHead();
    QCOMPARE(doc.boxes().size(), 2);

    // Test double click simulation on a commit
    QMetaObject::invokeMethod(&dock, "onCommitDoubleClicked", Q_ARG(GitCommitInfo, oldestActionCommit));
    QCoreApplication::processEvents();
    QCOMPARE(doc.boxes().size(), 1);
}

void TestProject::testGitConfigSettingsPersistence()
{
    QString testCfgPath = m_tempDir.filePath("test_app_config.json");
    AppConfig &cfg = AppConfig::instance();
    cfg.setConfigFilePath(testCfgPath);

    cfg.git().authorName = QStringLiteral("Alice Tester");
    cfg.git().authorEmail = QStringLiteral("alice@bentopack.test");
    QVERIFY(cfg.save());

    // Reset and reload
    cfg.resetToDefaults();
    QVERIFY(cfg.git().authorName.isEmpty());

    QVERIFY(cfg.load());
    QCOMPARE(cfg.git().authorName, QStringLiteral("Alice Tester"));
    QCOMPARE(cfg.git().authorEmail, QStringLiteral("alice@bentopack.test"));

    // Clean up custom path
    cfg.setConfigFilePath(QString());
}

void TestProject::testGitAuthorIdentityInCommits()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }

    SessionManager session;
    session.setAuthorIdentity(QStringLiteral("Bob SpriteMaker"), QStringLiteral("bob@pixelart.org"));
    QCOMPARE(session.authorName(), QStringLiteral("Bob SpriteMaker"));
    QCOMPARE(session.authorEmail(), QStringLiteral("bob@pixelart.org"));

    QVERIFY(session.startNewSession(m_tempDir.filePath("custom_author.bento")));

    // Write a dummy file to commit
    QFile f(session.sessionProjectJsonPath());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"test\": true}");
    f.close();

    QVERIFY(session.gitCommit(QStringLiteral("Custom author commit")));

    QList<GitCommitInfo> log = session.gitLog();
    QVERIFY(!log.isEmpty());
    QCOMPARE(log.first().author, QStringLiteral("Bob SpriteMaker"));
    QCOMPARE(log.first().email, QStringLiteral("bob@pixelart.org"));

    session.closeCurrentSession(true);
}

void TestProject::testSettingsDialogSearchAndApply()
{
    SettingsDialog dlg;
    dlg.setCurrentPage(SettingsDialog::PageGit);
    QCOMPARE(dlg.windowTitle().isEmpty(), false);

    // Search filter test
    QMetaObject::invokeMethod(&dlg, "onSearchTextChanged", Q_ARG(QString, QStringLiteral("author")));
    QMetaObject::invokeMethod(&dlg, "applySettings");
}

QTEST_MAIN(TestProject)
#include "test_project.moc"

