#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QPainter>
#include <QImageReader>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include "model/spritedocument.h"
#include "controller/projectcontroller.h"
#include "config/appconfig.h"
#include "commands/commands.h"
#include "project/sessionmanager.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include "widgets/openmediaconflictdialog.h"
#include <QPushButton>

class TestControllerProject : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testProjectControllerOpenJson();
    void testProjectControllerOpenGif();
    void testProjectControllerOpenWebp();
    void testProjectControllerOpenBento();
    void testProjectControllerOpenNonExistent();
    void testProjectControllerRecentFiles();
    void testProjectControllerBackgroundRemoval();
    void testProjectControllerDominantBackgroundColorAndUndo();
    void testProjectControllerOpenAsync();
    void testProjectControllerRemoveBgAsync();
    void testUndoStackLimitAndImageStorage();
    void testUndoRedoGitHeadSync();
    void testUndoGitWhenUndoStackEmpty();
    void testGitBranchingAndRedoSelection();
    void testReplaceExistingAtlasWithMediaAndUndo();
    void testAddMediaAsNewLayersAndUndo();
    void testAddMediaAsCommonBackgroundAsync();
    void testOpenMediaConflictDialogChoices();

private:
    QString m_sampleDir;
};

void TestControllerProject::initTestCase()
{
    QStringList candidates = {
        QStringLiteral(SAMPLE_DIR),
        QDir::current().filePath(QStringLiteral("../sample")),
        QDir::current().filePath(QStringLiteral("../../sample")),
        QDir::current().filePath(QStringLiteral("sample"))
    };
    for (const QString &cand : candidates) {
        if (QFile::exists(cand + QStringLiteral("/hero.png")) ||
            QFile::exists(cand + QStringLiteral("/hero.webp")) ||
            QFile::exists(cand + QStringLiteral("/hero.bento"))) {
            m_sampleDir = QDir(cand).canonicalPath();
            break;
        }
    }
    if (m_sampleDir.isEmpty()) {
        m_sampleDir = QStringLiteral(SAMPLE_DIR);
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QString binPlugins = QDir(appDir).filePath(QStringLiteral("plugins"));
    ExtractorRegistry::instance().loadPlugins(binPlugins);
    FilterRegistry::instance().loadPlugins(binPlugins);
    ExtractorRegistry::instance().loadPlugins(appDir);
    FilterRegistry::instance().loadPlugins(appDir);
}

void TestControllerProject::cleanupTestCase()
{
}

void TestControllerProject::testProjectControllerOpenJson()
{
    QString jsonPath = m_sampleDir + QStringLiteral("/hero.json");
    if (!QFile::exists(jsonPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    QString errorMsg;
    bool ok = controller.openFile(jsonPath, &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QCOMPARE(controller.currentFilePath(), jsonPath);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
}

void TestControllerProject::testProjectControllerOpenGif()
{
    QString gifPath = m_sampleDir + QStringLiteral("/hero.gif");
    if (!QFile::exists(gifPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);

    bool ok = controller.openFile(gifPath);
    QVERIFY(ok);
    QCOMPARE(spyLoaded.count(), 1);
    QVERIFY(doc.frameCount() > 1);
}

void TestControllerProject::testProjectControllerOpenWebp()
{
    QString webpPath = m_sampleDir + QStringLiteral("/hero.webp");
    if (!QFile::exists(webpPath)) {
        QSKIP("Sample file not present.");
    }

    if (!QImageReader::supportedImageFormats().contains("webp")) {
        QSKIP("WebP imageformat plugin not installed in Qt environment.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);

    bool ok = controller.openFile(webpPath);
    QVERIFY(ok);
    QCOMPARE(spyLoaded.count(), 1);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
}

void TestControllerProject::testProjectControllerOpenBento()
{
    QString bentoPath = m_sampleDir + QStringLiteral("/hero.bento");
    if (!QFile::exists(bentoPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);

    bool ok = controller.openFile(bentoPath);
    QVERIFY(ok);
    QCOMPARE(spyLoaded.count(), 1);
    QVERIFY(doc.frameCount() > 0);
}

void TestControllerProject::testProjectControllerOpenNonExistent()
{
    SpriteDocument doc;
    ProjectController controller(&doc);

    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    QString errorMsg;
    bool ok = controller.openFile(QStringLiteral("non_existent_file_98765.json"), &errorMsg);
    QVERIFY(!ok);
    QCOMPARE(spyLoaded.count(), 0);
    QCOMPARE(spyError.count(), 1);
    QVERIFY(!errorMsg.isEmpty());
}

void TestControllerProject::testProjectControllerRecentFiles()
{
    SpriteDocument doc;
    ProjectController controller(&doc);

    controller.clearRecentFiles();
    QVERIFY(controller.recentFiles().isEmpty());

    // Create 12 temporary dummy files
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QStringList createdFiles;
    for (int i = 1; i <= 12; ++i) {
        QString fpath = tempDir.filePath(QStringLiteral("file_%1.png").arg(i));
        QFile f(fpath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("dummy");
        f.close();
        createdFiles.append(fpath);
        controller.addRecentFile(fpath);
    }

    QStringList recent = controller.recentFiles();
    // Maximum 10 recent files
    QCOMPARE(recent.size(), 10);
    // Most recently added is at index 0
    QCOMPARE(recent.first(), createdFiles.last());

    controller.clearRecentFiles();
    QVERIFY(controller.recentFiles().isEmpty());
}

void TestControllerProject::testProjectControllerBackgroundRemoval()
{
    // Create an image: red background (255, 0, 0), with a blue 10x10 square
    QImage testImg(40, 40, QImage::Format_ARGB32);
    testImg.fill(qRgb(255, 0, 0));

    QPainter p(&testImg);
    p.fillRect(10, 10, 10, 10, QColor(0, 0, 255));
    p.end();

    QImage cleaned = ProjectController::removeBackgroundFromImage(testImg, 10);
    QVERIFY(!cleaned.isNull());

    // Corner pixel (background) should now be transparent
    QRgb cornerPixel = cleaned.pixel(0, 0);
    QCOMPARE(qAlpha(cornerPixel), 0);

    // Blue square pixel should still be fully opaque blue
    QRgb centerPixel = cleaned.pixel(15, 15);
    QCOMPARE(qAlpha(centerPixel), 255);
    QCOMPARE(qBlue(centerPixel), 255);

    // Test controller removeAtlasBackgroundAndRefresh with multi-sprite document
    SpriteDocument doc;
    QImage testImg2(60, 60, QImage::Format_ARGB32);
    testImg2.fill(qRgb(255, 0, 0));
    QPainter p2(&testImg2);
    p2.fillRect(5, 5, 10, 10, QColor(0, 0, 255));
    p2.fillRect(35, 35, 10, 10, QColor(0, 255, 0));
    p2.end();

    doc.setAtlas(testImg2);
    ProjectController controller(&doc);
    QSignalSpy spyBg(&controller, &ProjectController::backgroundRemoved);

    bool ok = controller.removeAtlasBackgroundAndRefresh(10, 5, false, 0.5);
    QVERIFY(ok);
    QCOMPARE(spyBg.count(), 1);
    QCOMPARE(doc.frameCount(), 2);
}

void TestControllerProject::testProjectControllerDominantBackgroundColorAndUndo()
{
    // 1. Dominant background color detection
    QImage testImg(60, 60, QImage::Format_ARGB32);
    testImg.fill(qRgb(255, 0, 128)); // distinctive pinkish magenta
    QPainter p(&testImg);
    p.fillRect(10, 10, 8, 8, QColor(0, 255, 0));
    p.fillRect(30, 30, 8, 8, QColor(0, 0, 255));
    p.end();

    QRgb detectedBg = ProjectController::detectDominantBackgroundColor(testImg);
    QCOMPARE(qRed(detectedBg), 255);
    QCOMPARE(qGreen(detectedBg), 0);
    QCOMPARE(qBlue(detectedBg), 128);

    // 2. RemoveBackgroundCommand test with undo and redo
    SpriteDocument doc;
    doc.setAtlas(testImg);
    QList<QImage> origFrames = { testImg.copy(10, 10, 8, 8) };
    SpriteBox box1;
    box1.rect = QRect(10, 10, 8, 8);
    box1.index = 0;
    QList<SpriteBox> origBoxes = { box1 };
    doc.setFrames(origFrames, origBoxes);
    QCOMPARE(doc.frameCount(), 1);

    // Prepare new state
    QImage cleaned = ProjectController::removeBackgroundFromImage(testImg, 10);
    QList<QImage> newFrames = {
        cleaned.copy(10, 10, 8, 8),
        cleaned.copy(30, 30, 8, 8)
    };
    SpriteBox box2;
    box2.rect = QRect(30, 30, 8, 8);
    box2.index = 1;
    QList<SpriteBox> newBoxes = { box1, box2 };

    QUndoStack undoStack;
    undoStack.push(new RemoveBackgroundCommand(&doc, cleaned, newFrames, newBoxes));

    // After push, document should have newAtlas and 2 frames
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 0);

    // Undo should restore original atlas and 1 frame
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(doc.atlas().pixel(0, 0), qRgb(255, 0, 128));

    // Redo should re-apply new atlas and 2 frames
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 0);
}

void TestControllerProject::testProjectControllerOpenAsync()
{
    QString pngPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(pngPath)) pngPath = m_sampleDir + QStringLiteral("/hero.webp");
    if (!QFile::exists(pngPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController controller(&doc, &undoStack);

    QSignalSpy spyStarted(&controller, &ProjectController::processingStarted);
    QSignalSpy spyLoaded(&controller, &ProjectController::fileLoaded);
    QSignalSpy spyFinished(&controller, &ProjectController::processingFinished);
    QSignalSpy spyError(&controller, &ProjectController::fileLoadError);

    controller.openFileAsync(pngPath);

    // If an error occurred synchronously (e.g. decoder missing), fail immediately with error message
    if (spyError.count() > 0) {
        QFAIL(qPrintable(spyError.first().at(1).toString()));
    }

    // Wait for the async worker thread and main-thread finish
    QVERIFY2(spyLoaded.wait(10000), spyError.isEmpty() ? "Timeout waiting for fileLoaded signal (10s)" : qPrintable(spyError.first().at(1).toString()));
    QCOMPARE(spyStarted.count(), 1);
    QCOMPARE(spyLoaded.count(), 1);
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(spyError.count(), 0);
    QCOMPARE(controller.currentFilePath(), pngPath);
    QVERIFY(!doc.atlas().isNull());
    QVERIFY(doc.frameCount() > 0);
}

void TestControllerProject::testProjectControllerRemoveBgAsync()
{
    SpriteDocument doc;
    QImage testImg(60, 60, QImage::Format_ARGB32);
    testImg.fill(qRgb(255, 0, 0));
    QPainter p(&testImg);
    p.fillRect(5, 5, 10, 10, QColor(0, 0, 255));
    p.fillRect(35, 35, 10, 10, QColor(0, 255, 0));
    p.end();

    doc.setAtlas(testImg);
    ProjectController controller(&doc);

    QSignalSpy spyStarted(&controller, &ProjectController::processingStarted);
    QSignalSpy spyBg(&controller, &ProjectController::backgroundRemoved);
    QSignalSpy spyFinished(&controller, &ProjectController::processingFinished);

    controller.removeAtlasBackgroundAndRefreshAsync(10, 5, false, 0.5);

    QVERIFY2(spyBg.wait(10000), "Timeout waiting for backgroundRemoved signal (10s)");
    QCOMPARE(spyStarted.count(), 1);
    QCOMPARE(spyBg.count(), 1);
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(doc.frameCount(), 2);
    // Background pixel (0,0) must now be transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(0, 0)), 0);
}

void TestControllerProject::testUndoStackLimitAndImageStorage()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Verify AppConfig undoLimit default is 50
    QCOMPARE(AppConfig::instance().project().undoLimit, 50);

    QUndoStack undoStack;
    undoStack.setUndoLimit(AppConfig::instance().project().undoLimit);
    QCOMPARE(undoStack.undoLimit(), 50);

    // Populate initial slices
    for (int i = 0; i < 65; ++i) {
        doc.addSlice(QRect(0, 0, 5, 5));
    }

    // Push 60 commands into undoStack to verify limit capping at 50
    for (int i = 0; i < 60; ++i) {
        undoStack.push(new DeleteFramesCommand(&doc, {doc.frameCount() - 1}));
    }

    // QUndoStack::count() reflects current history size capped by undoLimit
    QCOMPARE(undoStack.count(), 50);

    // Test undoing and redoing without texture or memory issues
    QVERIFY(undoStack.canUndo());
    undoStack.undo();
    QCOMPARE(undoStack.count(), 50);
    undoStack.redo();
    QCOMPARE(undoStack.count(), 50);
}

void TestControllerProject::testUndoRedoGitHeadSync()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    if (!QFile::exists(heroPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    // Real-world initial state: open atlas
    QVERIFY(pc.openFile(heroPath));
    QString hash0 = pc.sessionManager()->gitHeadCommitHash();
    QVERIFY(!hash0.isEmpty());
    QCOMPARE(pc.undoCommitHistory().value(0), hash0);

    // 1. Perform first action via undoStack
    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 16, 16)));
    QCOMPARE(undoStack.index(), 1);
    QString hash1 = pc.sessionManager()->gitHeadCommitHash();
    QVERIFY(!hash1.isEmpty());
    QVERIFY(hash1 != hash0);
    QCOMPARE(pc.undoCommitHistory().value(1), hash1);

    // 2. Perform second action via undoStack
    undoStack.push(new AddSliceCommand(&doc, QRect(16, 16, 16, 16)));
    QCOMPARE(undoStack.index(), 2);
    QString hash2 = pc.sessionManager()->gitHeadCommitHash();
    QVERIFY(!hash2.isEmpty());
    QVERIFY(hash2 != hash1);
    QCOMPARE(pc.undoCommitHistory().value(2), hash2);

    int commitCountBeforeUndo = pc.sessionManager()->gitLog().size();

    // 3. UNDO: Should step back HEAD to hash1 WITHOUT creating any new commit!
    undoStack.undo();
    QCOMPARE(undoStack.index(), 1);
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash1);
    // Number of commits in gitLog must NOT increase!
    QCOMPARE(pc.sessionManager()->gitLog().size(), commitCountBeforeUndo);

    // 4. UNDO again: Should step back HEAD to hash0
    undoStack.undo();
    QCOMPARE(undoStack.index(), 0);
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash0);
    QCOMPARE(pc.sessionManager()->gitLog().size(), commitCountBeforeUndo);

    // 5. REDO: Should advance HEAD to hash1 without creating a new commit
    undoStack.redo();
    QCOMPARE(undoStack.index(), 1);
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash1);
    QCOMPARE(pc.sessionManager()->gitLog().size(), commitCountBeforeUndo);

    // 6. REDO again: Should advance HEAD to hash2
    undoStack.redo();
    QCOMPARE(undoStack.index(), 2);
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash2);
    QCOMPARE(pc.sessionManager()->gitLog().size(), commitCountBeforeUndo);
}

void TestControllerProject::testUndoGitWhenUndoStackEmpty()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    if (!QFile::exists(heroPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QVERIFY(pc.openFile(heroPath));
    QString initialHash = pc.sessionManager()->gitHeadCommitHash();
    int initialCount = doc.frameCount();

    // Perform two actions
    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 16, 16)));
    QString hash1 = pc.sessionManager()->gitHeadCommitHash();
    undoStack.push(new AddSliceCommand(&doc, QRect(16, 16, 16, 16)));
    QString hash2 = pc.sessionManager()->gitHeadCommitHash();

    // Now simulate closing/reopening or stack clearing
    undoStack.clear();
    QCOMPARE(undoStack.count(), 0);
    QCOMPARE(undoStack.canUndo(), false);

    // HEAD is at hash2. Does HEAD have a parent in Git? YES (hash1)
    QVERIFY(pc.canUndoGit());
    QCOMPARE(pc.sessionManager()->gitParentCommitHash(hash2), hash1);

    // Executing undoGit() steps back to hash1 and reloads document
    QVERIFY(pc.undoGit());
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash1);
    QCOMPARE(doc.frameCount(), initialCount + 1); // 1 extra slice in hash1

    // At hash1, can we redo back to hash2?
    QVERIFY(pc.canRedoGit());
    QVERIFY(pc.redoGit(hash2));
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), hash2);
    QCOMPARE(doc.frameCount(), initialCount + 2); // 2 extra slices in hash2
}

void TestControllerProject::testGitBranchingAndRedoSelection()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    if (!QFile::exists(heroPath)) {
        QSKIP("Sample file not present.");
    }

    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QVERIFY(pc.openFile(heroPath));
    QString baseHash = pc.sessionManager()->gitHeadCommitHash();
    int initialCount = doc.frameCount();

    // Branch 1: Add slice at (0, 0, 16, 16)
    undoStack.push(new AddSliceCommand(&doc, QRect(0, 0, 16, 16)));
    QString branch1Hash = pc.sessionManager()->gitHeadCommitHash();
    QVERIFY(!branch1Hash.isEmpty());

    // Step back to baseHash using undo
    undoStack.undo();
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), baseHash);

    // Branch 2: Add different slice at (50, 50, 20, 20) -> should bifurcate
    undoStack.push(new AddSliceCommand(&doc, QRect(50, 50, 20, 20)));
    QString branch2Hash = pc.sessionManager()->gitHeadCommitHash();
    QVERIFY(!branch2Hash.isEmpty());
    QVERIFY(branch2Hash != branch1Hash);

    // Step back to baseHash
    undoStack.undo();
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), baseHash);

    // At baseHash, it now has 2 direct children in Git: branch1Hash and branch2Hash!
    QVERIFY(pc.hasMultipleRedoBranches());
    QList<GitCommitInfo> branches = pc.redoBranches();
    QCOMPARE(branches.size(), 2);

    QStringList childHashes;
    for (const GitCommitInfo &b : branches) {
        childHashes.append(b.hash);
    }
    QVERIFY(childHashes.contains(branch1Hash));
    QVERIFY(childHashes.contains(branch2Hash));

    // Restore branch 1 explicitly
    QVERIFY(pc.redoGit(branch1Hash));
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), branch1Hash);
    QCOMPARE(doc.frameCount(), initialCount + 1);
    QCOMPARE(doc.box(initialCount).rect, QRect(0, 0, 16, 16));

    // Step back to baseHash, then restore branch 2 explicitly
    QVERIFY(pc.undoGit());
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), baseHash);
    QVERIFY(pc.redoGit(branch2Hash));
    QCOMPARE(pc.sessionManager()->gitHeadCommitHash(), branch2Hash);
    QCOMPARE(doc.frameCount(), initialCount + 1);
    QCOMPARE(doc.box(initialCount).rect, QRect(50, 50, 20, 20));
}

void TestControllerProject::testReplaceExistingAtlasWithMediaAndUndo()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) {
        heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    }
    QString tilesPath = m_sampleDir + QStringLiteral("/tiles.png");
    if (!QFile::exists(heroPath) || !QFile::exists(tilesPath)) {
        QSKIP("Required sample images hero/tiles not found");
    }

    QString err;
    QVERIFY(pc.openFile(heroPath, &err));
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(!doc.atlas().isNull());

    QImage oldAtlas = doc.atlas();
    int oldFrameCount = doc.frameCount();

    // 1. Replace existing atlas with tiles.png
    QVERIFY(pc.replaceExistingAtlasWithMedia(tilesPath, &err));
    QVERIFY(doc.frameCount() > 0);
    QVERIFY(doc.atlas() != oldAtlas);

    // 2. Undo: should restore hero atlas and frames
    undoStack.undo();
    QCOMPARE(doc.atlas(), oldAtlas);
    QCOMPARE(doc.frameCount(), oldFrameCount);

    // 3. Redo: should re-apply tiles atlas
    undoStack.redo();
    QVERIFY(doc.atlas() != oldAtlas);
}

void TestControllerProject::testAddMediaAsNewLayersAndUndo()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) {
        heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    }
    QString tilesPath = m_sampleDir + QStringLiteral("/tiles.png");
    if (!QFile::exists(heroPath) || !QFile::exists(tilesPath)) {
        QSKIP("Required sample images hero/tiles not found");
    }

    QString err;
    QVERIFY(pc.openFile(heroPath, &err));
    QCOMPARE(doc.layerCount(), 0);

    // 1. Add tiles as new layers (Choice 3: Atlas Layer)
    QVERIFY(pc.addMediaAsNewLayers(tilesPath, &err, false));
    QVERIFY(doc.hasLayers());
    // Initial content promoted to "Base", plus new layer for tiles => 2 layers
    QCOMPARE(doc.layerCount(), 2);
    QCOMPARE(doc.layer(0).name, QStringLiteral("Base"));
    QVERIFY(doc.hasFrameCels(0));
    // Verify that cel is sliced according to box(0).rect size, not the full tiles image size!
    QCOMPARE(doc.frameCels(0).at(1).image.size(), doc.box(0).rect.size());

    // 2. Undo: should restore to 0 layers (flat document)
    undoStack.undo();
    QCOMPARE(doc.layerCount(), 0);
    QVERIFY(!doc.hasLayers());

    // 3. Redo: should re-apply the 2 layers
    undoStack.redo();
    QCOMPARE(doc.layerCount(), 2);
    QVERIFY(doc.hasLayers());
}

void TestControllerProject::testAddMediaAsCommonBackgroundAsync()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    ProjectController pc(&doc, &undoStack);

    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) {
        heroPath = m_sampleDir + QStringLiteral("/hero.webp");
    }
    QString tilesPath = m_sampleDir + QStringLiteral("/tiles.png");
    if (!QFile::exists(heroPath) || !QFile::exists(tilesPath)) {
        QSKIP("Required sample images hero/tiles not found");
    }

    QString err;
    QVERIFY(pc.openFile(heroPath, &err));
    QCOMPARE(doc.layerCount(), 0);

    QSignalSpy spyLoaded(&pc, &ProjectController::fileLoaded);
    QSignalSpy spyProgress(&pc, &ProjectController::progressChanged);
    QSignalSpy spyFinished(&pc, &ProjectController::processingFinished);

    // Add tiles as common background asynchronously (Case 4)
    pc.addMediaAsNewLayersAsync(tilesPath, true);
    QVERIFY(spyFinished.wait(5000));
    QCOMPARE(spyLoaded.count(), 1);

    // Verify 100% progress was reported
    QVERIFY(!spyProgress.isEmpty());
    QCOMPARE(spyProgress.last().at(0).toInt(), 100);

    // Initial content promoted to "Base", plus background layer at zOrder 0 => 2 layers
    QCOMPARE(doc.layerCount(), 2);
    QVERIFY(doc.hasLayers());
    QCOMPARE(doc.layer(0).zOrder, 0);
    QVERIFY(doc.layer(0).name.contains(QStringLiteral("Background")));
    QCOMPARE(doc.layer(1).name, QStringLiteral("Base"));
    QVERIFY(doc.hasFrameCels(0));
    // Verify background cel is scaled to frame size
    QCOMPARE(doc.frameCels(0).at(0).image.size(), doc.box(0).rect.size());

    // Undo: should restore to 0 layers
    undoStack.undo();
    QCOMPARE(doc.layerCount(), 0);
    QVERIFY(!doc.hasLayers());

    // Redo: should re-apply the 2 layers
    undoStack.redo();
    QCOMPARE(doc.layerCount(), 2);
    QVERIFY(doc.hasLayers());
}

void TestControllerProject::testOpenMediaConflictDialogChoices()
{
    OpenMediaConflictDialog dlg(QStringLiteral("sample/hero.png"));
    QCOMPARE(dlg.selectedChoice(), OpenMediaConflictDialog::Choice::CloseAndOpenNew);

    QPushButton *btn1 = dlg.findChild<QPushButton*>(QStringLiteral("btnChoiceCloseAndOpen"));
    QPushButton *btn2 = dlg.findChild<QPushButton*>(QStringLiteral("btnChoiceReplaceAtlas"));
    QPushButton *btn3 = dlg.findChild<QPushButton*>(QStringLiteral("btnChoiceAddNewLayers"));
    QPushButton *btn4 = dlg.findChild<QPushButton*>(QStringLiteral("btnChoiceAddCommonBackground"));
    QPushButton *btnCancel = dlg.findChild<QPushButton*>(QStringLiteral("btnCancel"));

    QVERIFY(btn1 != nullptr);
    QVERIFY(btn2 != nullptr);
    QVERIFY(btn3 != nullptr);
    QVERIFY(btn4 != nullptr);
    QVERIFY(btnCancel != nullptr);

    // Simulate clicking Option 2
    btn2->click();
    QCOMPARE(dlg.selectedChoice(), OpenMediaConflictDialog::Choice::ReplaceExistingAtlas);
    QCOMPARE(dlg.result(), QDialog::Accepted);

    // Simulate clicking Option 3
    OpenMediaConflictDialog dlg3(QStringLiteral("sample/hero.png"));
    QPushButton *b3 = dlg3.findChild<QPushButton*>(QStringLiteral("btnChoiceAddNewLayers"));
    b3->click();
    QCOMPARE(dlg3.selectedChoice(), OpenMediaConflictDialog::Choice::AddNewLayers);
    QCOMPARE(dlg3.result(), QDialog::Accepted);

    // Simulate clicking Option 4
    OpenMediaConflictDialog dlg4(QStringLiteral("sample/hero.png"));
    QPushButton *b4 = dlg4.findChild<QPushButton*>(QStringLiteral("btnChoiceAddCommonBackground"));
    b4->click();
    QCOMPARE(dlg4.selectedChoice(), OpenMediaConflictDialog::Choice::AddAsCommonBackground);
    QCOMPARE(dlg4.result(), QDialog::Accepted);

    // Simulate clicking Cancel
    OpenMediaConflictDialog dlgCancel(QStringLiteral("sample/hero.png"));
    QPushButton *bc = dlgCancel.findChild<QPushButton*>(QStringLiteral("btnCancel"));
    bc->click();
    QCOMPARE(dlgCancel.result(), QDialog::Rejected);
}

int main(int argc, char *argv[])
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    QApplication app(argc, argv);
    TestControllerProject tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controller_project.moc"
