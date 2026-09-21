#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QGraphicsView>
#include <QUndoStack>
#include <QContextMenuEvent>
#include <QTranslator>
#include <QSpinBox>
#include <QCheckBox>
#include <QMainWindow>
#include <QMenu>
#include <QDockWidget>
#include <QSettings>

#include "model/spritedocument.h"
#include "controller/projectcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/atlasviewcontroller.h"
#include "animation/animationplayer.h"
#include "config/appconfig.h"
#include "commands/commands.h"
#include "atlasboxitem.h"
#include "project/projectmanager.h"
#include "project/sessionmanager.h"
#include "filters/filterregistry.h"
#include "extractor/extractorregistry.h"
#include "backgroundremovaldialog.h"
#include "despillfilterdialog.h"
#include "outlinefilterdialog.h"
#include "colorswapfilterdialog.h"
#include "coloradjustfilterdialog.h"
#include "pixelrescalefilterdialog.h"
#include "retropalettefilterdialog.h"
#include "atlaspackingdialog.h"
#include "atlaspackingfilter.h"
#include "commands/filtercommands.h"

class TestControllers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // AppConfig tests
    void testAppConfigDefaults();
    void testAppConfigSaveAndLoad();
    void testAppConfigCorruptJsonFallback();

    // ProjectController tests
    void testProjectControllerOpenJson();
    void testProjectControllerOpenGif();
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

    // AnimationController tests
    void testAnimationControllerPlayback();
    void testAnimationControllerCreateAnimation();
    void testAnimationControllerReverseAnimation();
    void testAnimationControllerRemoveAnimation();
    void testAnimationControllerCurrentSelection();
    void testAnimationControllerAutoPlay();
    void testAnimationPlayerPingPongAndOnce();
    void testAnimationDuplicationAndRename();
    void testTimelineReorderFrames();
    void testAnimationLoopModePersistence();
    void testTransportSpeedTimingStability();
    void testAnimationControllerPivotAlignment();
    void testAnimationControllerReticleToggle();
    void testAtlasBoxItemPivotDrag();
    void testAnimationPreviewZoomAndFit();
    void testAnimationPreviewPivotInteraction();
    void testAnimationPolygonMasking();

    // AtlasViewController tests
    void testAtlasViewControllerToolMode();
    void testAtlasViewControllerZoom();
    void testAtlasViewControllerBoxSync();
    void testAtlasViewControllerSelection();
    void testAtlasViewControllerNudge();
    void testAtlasViewControllerTrimAndMerge();
    void testAtlasViewControllerErasePixels();
    void testAtlasViewControllerMarqueeSelection();
    void testAtlasViewControllerContextMenuSignals();
    void testControllerCrossSyncNoRecursion();
    void testAtlasViewControllerMultiSelectAndDelete();
    void testAtlasViewControllerMouseCenteredZoom();
    void testI18nKeyTranslations();
    void testAtlasBoxItemHandleCosmeticSize();
    void testAtlasViewControllerGroupDrag();
    void testAtlasViewControllerContinuousSlice();
    void testDockStatePersistence();
    void testSelectionOrderPreserved();

    // Filter & Plugin Architecture tests
    void testFilterRegistry();
    void testDespillFilterAlgorithm();
    void testOutlineFilterAlgorithm();
    void testColorSwapFilterAlgorithm();
    void testColorAdjustFilterAlgorithm();
    void testPixelRescaleFilterAlgorithm();
    void testRetroPaletteFilterAlgorithm();
    void testApplyFilterCommandUndoRedo();
    void testFilterAutoDetectBoxes();
    void testAtlasPackingFilterInteractive();

private:
    QString m_sampleDir;
};

void TestControllers::initTestCase()
{
    QStringList candidates = {
        QStringLiteral(SAMPLE_DIR),
        QDir::current().filePath(QStringLiteral("../sample")),
        QDir::current().filePath(QStringLiteral("../../sample")),
        QDir::current().filePath(QStringLiteral("sample"))
    };
    for (const QString &cand : candidates) {
        if (QFile::exists(cand + QStringLiteral("/hero.png")) || QFile::exists(cand + QStringLiteral("/ryu.png"))) {
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

void TestControllers::cleanupTestCase()
{
}

// -----------------------------------------------------------------------------
// AppConfig Tests
// -----------------------------------------------------------------------------

void TestControllers::testAppConfigDefaults()
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

void TestControllers::testAppConfigSaveAndLoad()
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

void TestControllers::testAppConfigCorruptJsonFallback()
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

// -----------------------------------------------------------------------------
// ProjectController Tests
// -----------------------------------------------------------------------------

void TestControllers::testProjectControllerOpenJson()
{
    QString jsonPath = m_sampleDir + QStringLiteral("/hero.json");
    if (!QFile::exists(jsonPath)) jsonPath = m_sampleDir + QStringLiteral("/ryu.json");
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

void TestControllers::testProjectControllerOpenGif()
{
    QString gifPath = m_sampleDir + QStringLiteral("/hero.gif");
    if (!QFile::exists(gifPath)) gifPath = m_sampleDir + QStringLiteral("/ryu_hd.gif");
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

void TestControllers::testProjectControllerOpenNonExistent()
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

void TestControllers::testProjectControllerRecentFiles()
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

void TestControllers::testProjectControllerBackgroundRemoval()
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

void TestControllers::testProjectControllerDominantBackgroundColorAndUndo()
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

void TestControllers::testProjectControllerOpenAsync()
{
    QString pngPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(pngPath)) pngPath = m_sampleDir + QStringLiteral("/ryu.png");
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

void TestControllers::testProjectControllerRemoveBgAsync()
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

void TestControllers::testUndoStackLimitAndImageStorage()
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

void TestControllers::testUndoRedoGitHeadSync()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/ryu.png");
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

void TestControllers::testUndoGitWhenUndoStackEmpty()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/ryu.png");
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

void TestControllers::testGitBranchingAndRedoSelection()
{
    if (!SessionManager::isGitAvailable()) {
        QSKIP("Git integration not compiled in.");
    }
    QString heroPath = m_sampleDir + QStringLiteral("/hero.png");
    if (!QFile::exists(heroPath)) heroPath = m_sampleDir + QStringLiteral("/ryu.png");
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

// -----------------------------------------------------------------------------
// AnimationController Tests
// -----------------------------------------------------------------------------

void TestControllers::testAnimationControllerPlayback()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);
    animCtrl.player()->setSequence({0, 1}, 12);

    QCOMPARE(animCtrl.fps(), 12);
    animCtrl.setFps(24);
    QCOMPARE(animCtrl.fps(), 24);

    QSignalSpy spyState(&animCtrl, &AnimationController::playbackStateChanged);

    QVERIFY(!animCtrl.isPlaying());
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    QCOMPARE(spyState.count(), 1);

    animCtrl.pause();
    QVERIFY(!animCtrl.isPlaying());

    animCtrl.togglePlayPause();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.togglePlayPause();
    QVERIFY(!animCtrl.isPlaying());
}

void TestControllers::testAnimationControllerCreateAnimation()
{
    SpriteDocument doc;
    // Add 4 dummy frames
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::black);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));
    doc.addSlice(QRect(16, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    QSignalSpy spyAnim(&animCtrl, &AnimationController::currentAnimationChanged);

    animCtrl.createAnimation(QStringLiteral("walk"), {0, 1, 2}, 15);

    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2}));
    QCOMPARE(doc.animation(QStringLiteral("walk")).fps, 15);
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("walk"));

    // Undo should remove animation
    undoStack.undo();
    QVERIFY(!doc.hasAnimation(QStringLiteral("walk")));

    // Redo should restore animation
    undoStack.redo();
    QVERIFY(doc.hasAnimation(QStringLiteral("walk")));
}

void TestControllers::testAnimationControllerReverseAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(0, 16, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("attack"), {0, 1, 2}, 10);
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));

    animCtrl.reverseAnimationOrder();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{2, 1, 0}));

    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("attack")).frameIndices, (QList<int>{0, 1, 2}));
}

void TestControllers::testAnimationControllerRemoveAnimation()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("idle"), {0}, 8);
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));

    animCtrl.removeAnimation(QStringLiteral("idle"));
    QVERIFY(!doc.hasAnimation(QStringLiteral("idle")));

    undoStack.undo();
    QVERIFY(doc.hasAnimation(QStringLiteral("idle")));
}

void TestControllers::testAnimationControllerCurrentSelection()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    AnimationController animCtrl(&doc);

    QVERIFY(!animCtrl.hasCurrentAnimation());

    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.hasCurrentAnimation());
    QCOMPARE(animCtrl.currentAnimationName(), QStringLiteral("current"));

    animCtrl.removeCurrentAnimation();
    QVERIFY(!animCtrl.hasCurrentAnimation());
}

void TestControllers::testAnimationControllerAutoPlay()
{
    SpriteDocument doc;
    QImage img(48, 48, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(32, 0, 16, 16));

    AnimationController animCtrl(&doc);

    // Auto-play default should be true
    QVERIFY(AppConfig::instance().animation().autoPlayOnSelection);

    // 1. Selecting 2 frames automatically starts playback
    animCtrl.updateCurrentAnimation({0, 1});
    QVERIFY(animCtrl.isPlaying());

    // 2. Selecting 1 frame pauses playback
    animCtrl.updateCurrentAnimation({0});
    QVERIFY(!animCtrl.isPlaying());

    // 3. Starting manual play then changing selection preserves playback
    animCtrl.play();
    QVERIFY(animCtrl.isPlaying());
    animCtrl.updateCurrentAnimation({1, 2});
    QVERIFY(animCtrl.isPlaying());
}

void TestControllers::testAnimationPlayerPingPongAndOnce()
{
    AnimationPlayer player;
    player.setSequence({10, 20, 30}, 10);
    player.setLoopMode(AnimationPlayer::PingPong);
    QCOMPARE(player.loopMode(), AnimationPlayer::PingPong);

    // Initial frame is index 0 (frame 10)
    QCOMPARE(player.currentSequenceIndex(), 0);
    QCOMPARE(player.currentGlobalFrameIndex(), 10);

    // Step forward: 0 -> 1 -> 2
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);
    QCOMPARE(player.currentGlobalFrameIndex(), 20);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 2);
    QCOMPARE(player.currentGlobalFrameIndex(), 30);

    // Next step in PingPong bounces back: 2 -> 1 -> 0 -> 1 ...
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);
    QCOMPARE(player.currentGlobalFrameIndex(), 20);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 0);
    QCOMPARE(player.currentGlobalFrameIndex(), 10);

    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 1);

    // Navigation tests
    player.lastFrame();
    QCOMPARE(player.currentSequenceIndex(), 2);
    player.firstFrame();
    QCOMPARE(player.currentSequenceIndex(), 0);
    player.seek(1);
    QCOMPARE(player.currentSequenceIndex(), 1);

    // Test LoopMode::Once
    player.setLoopMode(AnimationPlayer::Once);
    player.firstFrame();
    player.play();
    QVERIFY(player.isPlaying());

    // Advance to last frame
    player.stepForward(); // 1
    QCOMPARE(player.currentSequenceIndex(), 1);
    player.stepForward(); // 2 (last frame)
    QCOMPARE(player.currentSequenceIndex(), 2);

    // In Once mode, advancing past the end stops playback and stays on last frame
    player.stepForward();
    QCOMPARE(player.currentSequenceIndex(), 2);
    QVERIFY(!player.isPlaying());
}

void TestControllers::testAnimationDuplicationAndRename()
{
    SpriteDocument doc;
    QImage img(48, 48, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));
    doc.addSlice(QRect(32, 0, 16, 16));

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("run"), {0, 1, 2}, 14, SpriteAnimation::PingPong);
    QVERIFY(doc.hasAnimation(QStringLiteral("run")));
    QCOMPARE(doc.animation(QStringLiteral("run")).loopMode, SpriteAnimation::PingPong);

    // Duplicate animation
    animCtrl.duplicateAnimation(QStringLiteral("run"));
    QString copyName = QStringLiteral("run_copy");
    QVERIFY(doc.hasAnimation(copyName));
    QCOMPARE(doc.animation(copyName).frameIndices, (QList<int>{0, 1, 2}));
    QCOMPARE(doc.animation(copyName).fps, 14);
    QCOMPARE(doc.animation(copyName).loopMode, SpriteAnimation::PingPong);

    // Undo duplication
    undoStack.undo();
    QVERIFY(!doc.hasAnimation(copyName));

    // Redo duplication
    undoStack.redo();
    QVERIFY(doc.hasAnimation(copyName));

    // Rename animation
    animCtrl.renameAnimation(copyName, QStringLiteral("run_fast"));
    QVERIFY(!doc.hasAnimation(copyName));
    QVERIFY(doc.hasAnimation(QStringLiteral("run_fast")));
    QCOMPARE(doc.animation(QStringLiteral("run_fast")).fps, 14);

    // Undo rename
    undoStack.undo();
    QVERIFY(doc.hasAnimation(copyName));
    QVERIFY(!doc.hasAnimation(QStringLiteral("run_fast")));

    // Redo rename
    undoStack.redo();
    QVERIFY(!doc.hasAnimation(copyName));
    QVERIFY(doc.hasAnimation(QStringLiteral("run_fast")));
}

void TestControllers::testTimelineReorderFrames()
{
    SpriteDocument doc;
    QImage img(64, 64, QImage::Format_ARGB32);
    doc.setAtlas(img);
    for (int i = 0; i < 4; ++i) {
        doc.addSlice(QRect(i * 16, 0, 16, 16));
    }

    QUndoStack undoStack;
    AnimationController animCtrl(&doc, &undoStack);

    animCtrl.createAnimation(QStringLiteral("combo"), {0, 1, 2, 3}, 12);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{0, 1, 2, 3}));

    // Reorder sequence
    animCtrl.reorderAnimationFrames(QStringLiteral("combo"), {3, 0, 2, 1});
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));

    // Undo reorder
    undoStack.undo();
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{0, 1, 2, 3}));

    // Redo reorder
    undoStack.redo();
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));

    // Add and remove frame from animation
    animCtrl.addFrameToAnimation(QStringLiteral("combo"), 0);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1, 0}));

    animCtrl.removeFrameFromAnimation(QStringLiteral("combo"), 4);
    QCOMPARE(doc.animation(QStringLiteral("combo")).frameIndices, (QList<int>{3, 0, 2, 1}));
}

void TestControllers::testAnimationLoopModePersistence()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));
    doc.addSlice(QRect(16, 0, 16, 16));

    doc.addAnimation(QStringLiteral("loop_anim"), {0, 1}, 10, SpriteAnimation::Loop);
    doc.addAnimation(QStringLiteral("pingpong_anim"), {0, 1}, 12, SpriteAnimation::PingPong);
    doc.addAnimation(QStringLiteral("once_anim"), {0, 1}, 8, SpriteAnimation::Once);

    QByteArray json = ProjectManager::serializeDocumentToJson(doc, QStringLiteral("atlas.png"), 1.0, QPointF(0, 0));

    SpriteDocument restoredDoc;
    double zoom = 1.0;
    QPointF pan;
    QString errMsg;
    bool ok = ProjectManager::deserializeJsonToDocument(json, restoredDoc, QStringLiteral("."), &zoom, &pan, &errMsg);
    QVERIFY2(ok, qPrintable(errMsg));

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("loop_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("loop_anim")).loopMode, SpriteAnimation::Loop);

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("pingpong_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("pingpong_anim")).loopMode, SpriteAnimation::PingPong);

    QVERIFY(restoredDoc.hasAnimation(QStringLiteral("once_anim")));
    QCOMPARE(restoredDoc.animation(QStringLiteral("once_anim")).loopMode, SpriteAnimation::Once);
}

void TestControllers::testTransportSpeedTimingStability()
{
    SpriteDocument doc;
    QImage img(32, 32, QImage::Format_ARGB32);
    doc.setAtlas(img);
    doc.addSlice(QRect(0, 0, 16, 16));

    AnimationPlayer player;
    AnimationController animCtrl(&doc, nullptr, &player);

    QSlider scrubber(Qt::Horizontal);
    QLabel frameIndicator;
    animCtrl.attachScrubberSlider(&scrubber, &frameIndicator);

    QLabel fpsLabel;
    QSpinBox fpsSpinBox;
    QLabel timingLabel;

    fpsLabel.setFixedWidth(28);
    fpsSpinBox.setFixedWidth(55);
    timingLabel.setFixedWidth(120);
    timingLabel.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Initial widths
    QCOMPARE(fpsLabel.width(), 28);
    QCOMPARE(fpsSpinBox.width(), 55);
    QCOMPARE(timingLabel.width(), 120);

    // Connect FPS changes to update timingLabel exactly as MainWindow does
    connect(&animCtrl, &AnimationController::fpsChanged, [&](int fps) {
        fpsSpinBox.blockSignals(true);
        fpsSpinBox.setValue(fps);
        fpsSpinBox.blockSignals(false);
        timingLabel.setText(" -> Timing: " + QString::number(1000.0 / static_cast<double>(fps), 'g', 4) + "ms");
    });

    animCtrl.createAnimation(QStringLiteral("walk"), {0}, 10);

    // Test clicking across various FPS values within valid minFps..maxFps (1..60)
    const QList<int> testFpsList = {10, 9, 8, 12, 24, 30, 60, 1};
    for (int fpsVal : testFpsList) {
        animCtrl.setFps(fpsVal);
        QCOMPARE(fpsSpinBox.value(), fpsVal);

        // Verify the timing text matches expected ms
        QString expectedMs = QString::number(1000.0 / static_cast<double>(fpsVal), 'g', 4) + "ms";
        QVERIFY(timingLabel.text().contains(expectedMs));

        // CRITICAL: The widths of the widgets MUST remain strictly invariant
        QCOMPARE(fpsLabel.width(), 28);
        QCOMPARE(fpsSpinBox.width(), 55);
        QCOMPARE(timingLabel.width(), 120);
    }
}

// -----------------------------------------------------------------------------
// AtlasViewController Tests
// -----------------------------------------------------------------------------

void TestControllers::testAtlasViewControllerToolMode()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyTool(&atlasCtrl, &AtlasViewController::toolModeChanged);

    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);

    atlasCtrl.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolAddSlice);
    QCOMPARE(spyTool.count(), 1);

    atlasCtrl.setToolMode(AtlasViewController::ToolSelect);
    QCOMPARE(atlasCtrl.toolMode(), AtlasViewController::ToolSelect);
    QCOMPARE(spyTool.count(), 2);
}

void TestControllers::testAtlasViewControllerZoom()
{
    QGraphicsView view;
    SpriteDocument doc;
    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spyZoom(&atlasCtrl, &AtlasViewController::zoomChanged);

    atlasCtrl.setZoomFactor(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    QCOMPARE(spyZoom.count(), 1);

    // Test clamping limits: min 0.1, max 10.0
    atlasCtrl.setZoomFactor(0.01);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);

    atlasCtrl.setZoomFactor(50.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(1.0);
    atlasCtrl.zoomIn(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 2.0);
    atlasCtrl.zoomOut(2.0);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.0);
}

void TestControllers::testAtlasViewControllerBoxSync()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);

    QCOMPARE(atlasCtrl.boxCount(), 0);

    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));

    // framesChanged signal triggers syncAtlasBoxes
    QCOMPARE(atlasCtrl.boxCount(), 3);
}

void TestControllers::testAtlasViewControllerSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(20, 0, 20, 20));
    doc.addSlice(QRect(40, 0, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);

    QSignalSpy spySel(&atlasCtrl, &AtlasViewController::selectionChanged);

    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>{1});
    QCOMPARE(spySel.count(), 1);

    atlasCtrl.selectAll();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1, 2}));

    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());

    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.invertSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 2}));

    atlasCtrl.clearSelection();
    QCOMPARE(atlasCtrl.selectedBoxIndices(), QList<int>());
}

void TestControllers::testAtlasViewControllerNudge()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    atlasCtrl.setSelectedBoxIndices({0});

    // Nudge right 5px, down 3px
    atlasCtrl.nudgeSelectedBoxes(5, 3);
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));

    // Undo should return box to original pos
    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));

    // Redo should apply nudge again
    undoStack.redo();
    QCOMPARE(doc.box(0).rect, QRect(15, 13, 20, 20));
}

void TestControllers::testAtlasViewControllerTrimAndMerge()
{
    QGraphicsView view;
    SpriteDocument doc;
    // Create an atlas with an opaque 10x10 area inside a 30x30 bounding box
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);

    QPainter p(&atlas);
    p.fillRect(5, 5, 10, 10, Qt::red);
    p.fillRect(50, 50, 20, 20, Qt::blue);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 30, 30));    // Box 0: includes the 10x10 red area at (5,5)
    doc.addSlice(QRect(40, 40, 40, 40));  // Box 1: includes the 20x20 blue area at (50,50)

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Test Trim on Box 0
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.trimSelectedSlice(1);
    QCOMPARE(doc.box(0).rect, QRect(5, 5, 10, 10));

    undoStack.undo();
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 30, 30));

    // Test Merge Box 0 and Box 1
    atlasCtrl.setSelectedBoxIndices({0, 1});
    atlasCtrl.mergeSelectedSlices();
    QCOMPARE(doc.frameCount(), 1);

    undoStack.undo();
    QCOMPARE(doc.frameCount(), 2);
}

void TestControllers::testAtlasViewControllerErasePixels()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);

    // Paint an opaque red rectangle at (10, 10, 20, 20)
    QPainter p(&atlas);
    p.fillRect(10, 10, 20, 20, Qt::red);
    p.end();

    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20)); // Frame 0

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // Select box 0 and erase its pixels
    atlasCtrl.setSelectedBoxIndices({0});
    atlasCtrl.eraseSelectedSlicesPixels();

    // 1. Frame count is now 0
    QCOMPARE(doc.frameCount(), 0);

    // 2. Pixels inside the rect on atlas are now transparent
    QRgb pixelInside = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(pixelInside), 0);

    // 3. Pixel outside the rect remains white
    QRgb pixelOutside = doc.atlas().pixel(5, 5);
    QCOMPARE(qAlpha(pixelOutside), 255);

    // 4. Undo restores frame and original red pixels
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QRgb restoredPixel = doc.atlas().pixel(15, 15);
    QCOMPARE(qAlpha(restoredPixel), 255);
    QCOMPARE(qRed(restoredPixel), 255);

    // 5. Redo erases pixels again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(15, 15)), 0);
}

void TestControllers::testAtlasViewControllerMarqueeSelection()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // 4 boxes horizontally
    doc.addSlice(QRect(0, 0, 20, 20));    // 0
    doc.addSlice(QRect(30, 0, 20, 20));   // 1
    doc.addSlice(QRect(60, 0, 20, 20));   // 2
    doc.addSlice(QRect(90, 0, 20, 20));   // 3

    AtlasViewController atlasCtrl(&view, &doc);

    // Initial marquee selecting box 0 and 1
    atlasCtrl.startMarqueeSelection(QPointF(5, 5), Qt::NoModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(45, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expected01 = {0, 1};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expected01);

    // Additive marquee: adds box 2 and 3
    atlasCtrl.startMarqueeSelection(QPointF(55, 5), Qt::ControlModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(95, 15));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedAll = {0, 1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedAll);

    // Subtractive marquee: removes box 0
    atlasCtrl.startMarqueeSelection(QPointF(0, 0), Qt::ShiftModifier);
    atlasCtrl.updateMarqueeSelection(QPointF(25, 25));
    atlasCtrl.endMarqueeSelection();

    QList<int> expectedRest = {1, 2, 3};
    QCOMPARE(atlasCtrl.selectedBoxIndices(), expectedRest);
}

void TestControllers::testControllerCrossSyncNoRecursion()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(200, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    doc.addSlice(QRect(0, 0, 20, 20));
    doc.addSlice(QRect(30, 0, 20, 20));
    doc.addSlice(QRect(60, 0, 20, 20));
    doc.addSlice(QRect(90, 0, 20, 20));

    QUndoStack undoStack;
    AtlasViewController atlasCtrl(&view, &doc, &undoStack);
    AnimationController animCtrl(&doc, &undoStack);

    // Wire them identically to MainWindow::setupControllers()
    QObject::connect(&atlasCtrl, &AtlasViewController::selectionChanged,
                     &animCtrl, &AnimationController::updateCurrentAnimation);

    QObject::connect(&animCtrl, &AnimationController::framesSelectedInAnimation,
                     &atlasCtrl, [&atlasCtrl](const QList<int> &indices) {
        if (atlasCtrl.selectedBoxIndices() != indices) {
            atlasCtrl.setSelectedBoxIndices(indices);
        }
    });

    // 1. Selecting on atlas updates animation without infinite recursion
    atlasCtrl.setSelectedBoxIndices({0, 1});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 1}));
    QVERIFY(doc.hasAnimation(QStringLiteral("current")));
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{0, 1}));

    // 2. Selecting an explicit animation updates atlas selection
    animCtrl.createAnimation(QStringLiteral("run"), {2, 3}, 12);
    animCtrl.selectAnimation(QStringLiteral("run"));
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{2, 3}));

    // 3. Modifying atlas selection again does not disrupt animation list
    atlasCtrl.setSelectedBoxIndices({1});
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{1}));
}

void TestControllers::testAtlasViewControllerContextMenuSignals()
{
    QGraphicsView view;
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(10, 10, 20, 20));

    AtlasViewController atlasCtrl(&view, &doc);
    QSignalSpy spyAtlasMenu(&atlasCtrl, &AtlasViewController::atlasContextMenuRequested);

    // Context menu event on empty space (e.g. 80, 80)
    QContextMenuEvent emptyEvent(QContextMenuEvent::Mouse, QPoint(80, 80), view.viewport()->mapToGlobal(QPoint(80, 80)));
    QCoreApplication::sendEvent(view.viewport(), &emptyEvent);

    QCOMPARE(spyAtlasMenu.count(), 1);
    QCOMPARE(spyAtlasMenu.takeFirst().at(0).toPoint(), QPoint(80, 80));
}

void TestControllers::testAtlasViewControllerMultiSelectAndDelete()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;

    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    // Draw distinct colors in 5 regions
    for (int i = 0; i < 5; ++i) {
        QRect r(i * 20, 0, 15, 15);
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(atlas.scanLine(y));
            for (int x = r.left(); x <= r.right(); ++x) {
                line[x] = qRgba(50 * (i + 1), 0, 0, 255);
            }
        }
    }
    doc.setAtlas(atlas);

    for (int i = 0; i < 5; ++i) {
        doc.addSlice(QRect(i * 20, 0, 15, 15));
    }
    QCOMPARE(doc.frameCount(), 5);

    AtlasViewController atlasCtrl(&view, &doc, &undoStack);

    // 1. Multi-selection does not collapse
    atlasCtrl.setSelectedBoxIndices({1, 3});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{1, 3}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{1, 3}));

    // 2. Delete selected slices (slices 1 and 3)
    QSignalSpy framesChangedSpy(&doc, &SpriteDocument::framesChanged);
    atlasCtrl.deleteSelectedSlices();

    QCOMPARE(doc.frameCount(), 3);
    QVERIFY(framesChangedSpy.count() >= 1);
    // Remaining boxes should be original 0, 2, 4 (now at 0, 1, 2)
    QCOMPARE(doc.box(0).rect, QRect(0, 0, 15, 15));
    QCOMPARE(doc.box(1).rect, QRect(40, 0, 15, 15));
    QCOMPARE(doc.box(2).rect, QRect(80, 0, 15, 15));

    // 3. Undo restores all 5 frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 5);
    QCOMPARE(doc.box(1).rect, QRect(20, 0, 15, 15));
    QCOMPARE(doc.box(3).rect, QRect(60, 0, 15, 15));

    // 4. Redo deletes them again
    undoStack.redo();
    QCOMPARE(doc.frameCount(), 3);

    // 5. Erase pixels for slices 0 and 2 (which correspond to original 0 and 4)
    atlasCtrl.setSelectedBoxIndices({0, 2});
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{0, 2}));

    QSignalSpy atlasChangedSpy(&doc, &SpriteDocument::atlasChanged);
    atlasCtrl.eraseSelectedSlicesPixels();

    // Now only 1 frame remains (original 2, which was at index 1)
    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(doc.box(0).rect, QRect(40, 0, 15, 15));
    QVERIFY(atlasChangedSpy.count() >= 1);

    // Verify erased pixels are transparent
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 0);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 0);
    // Non-erased slice still has opaque pixels
    QCOMPARE(qAlpha(doc.atlas().pixel(45, 5)), 255);

    // 6. Undo restores both pixels and frames
    undoStack.undo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(qAlpha(doc.atlas().pixel(5, 5)), 255);
    QCOMPARE(qAlpha(doc.atlas().pixel(85, 5)), 255);
}

void TestControllers::testAtlasViewControllerMouseCenteredZoom()
{
    QGraphicsView view;
    view.resize(800, 600);
    view.show();
    QCoreApplication::processEvents();

    SpriteDocument doc;
    QImage atlas(1000, 1000, QImage::Format_ARGB32_Premultiplied);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    AtlasViewController atlasCtrl(&view, &doc);
    atlasCtrl.setZoomFactor(1.0);
    QCoreApplication::processEvents();

    // Zoom centered on a specific viewport point (e.g., 300, 250)
    QPointF mousePos(300.0, 250.0);
    QPointF sceneBefore = view.mapToScene(mousePos.toPoint());

    // Zoom in by factor 1.5
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 1.5);

    QPointF sceneAfter = view.mapToScene(mousePos.toPoint());
    qDebug() << "sceneBefore:" << sceneBefore << "sceneAfter:" << sceneAfter
             << "diff:" << (sceneAfter - sceneBefore);
    // The scene point mapped to the cursor must remain stationary (within 2 pixels tolerance)
    QVERIFY(qAbs(sceneAfter.x() - sceneBefore.x()) <= 2.0);
    QVERIFY(qAbs(sceneAfter.y() - sceneBefore.y()) <= 2.0);

    // Zoom out by factor 0.8 at another point
    QPointF mousePos2(150.0, 120.0);
    QPointF sceneBefore2 = view.mapToScene(mousePos2.toPoint());
    atlasCtrl.zoomAt(mousePos2, 0.8);
    QPointF sceneAfter2 = view.mapToScene(mousePos2.toPoint());
    QVERIFY(qAbs(sceneAfter2.x() - sceneBefore2.x()) <= 2.0);
    QVERIFY(qAbs(sceneAfter2.y() - sceneBefore2.y()) <= 2.0);

    // Check zoom limits with zoomAt
    atlasCtrl.setZoomFactor(10.0);
    atlasCtrl.zoomAt(mousePos, 1.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 10.0);

    atlasCtrl.setZoomFactor(0.1);
    atlasCtrl.zoomAt(mousePos, 0.5);
    QCOMPARE(atlasCtrl.zoomFactor(), 0.1);
}

void TestControllers::testI18nKeyTranslations()
{
#ifdef QM_DIR
    QString qmDir = QStringLiteral(QM_DIR);
    if (!QFile::exists(qmDir + QStringLiteral("/sprite_studio_fr_FR.qm"))) {
        if (QFile::exists(qmDir + QStringLiteral("/.qm/sprite_studio_fr_FR.qm"))) {
            qmDir = qmDir + QStringLiteral("/.qm");
        } else if (QFile::exists(QStringLiteral(":/i18n/sprite_studio_fr_FR.qm"))) {
            qmDir = QStringLiteral(":/i18n");
        }
    }

    // 1. Test French translation
    {
        QTranslator frTranslator;
        bool loaded = frTranslator.load(QStringLiteral("sprite_studio_fr_FR.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_fr_FR.qm from QM_DIR");

        QCoreApplication::installTranslator(&frTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("Fichier"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("Affichage"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("Barre d'outils principale"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("Aperçu de l'animation"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Ouvrir"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Enregistrer"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Sélectionner"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Créer une animation depuis la sélection"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("À propos"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("Préférences"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ Ajouter la sélection"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("Restaurer cette révision"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("Projet sans titre"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Les modifications de langue s'appliquent immédiatement."));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("&Filtres"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("Suppression d'arrière-plan"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("Aperçu en direct"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("Détection auto des boîtes"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("Ajustement des Couleurs (HSV & Contraste)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("Redimensionnement Pixel Art..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("Palette Rétro & Tramage (Dithering)..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("Géométrie & Transformations"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("Empaquetage d'Atlas (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("Empaquetage d'Atlas (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("Exporter l'Atlas & les Animations"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("Conserver l'agencement actuel (WYSIWYG — Tel quel)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — Recommandé)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("Forcer la Puissance de Deux (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("Efficacité d'empaquetage : Conservée telle quelle (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("Maillage polygonal 2D & Découpage serré"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("Tolérance d'approximation (ε) :"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("Appliquer à la sélection"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — SpriteStudio"), QStringLiteral("Éditeur de pixels — SpriteStudio"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("Crayon (Bresenham 1px continu) [P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("Ne pas enregistrer"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("Enregistrer"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("Appliquer"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("Annuler"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("Appliquer"));

        QCoreApplication::removeTranslator(&frTranslator);
    }

    // 2. Test English translation
    {
        QTranslator enTranslator;
        bool loaded = enTranslator.load(QStringLiteral("sprite_studio_en_US.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_en_US.qm from QM_DIR");

        QCoreApplication::installTranslator(&enTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("File"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("View"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("Main Toolbar"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("Animation Preview"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("&Open"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("&Save"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("Select & Edit"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("Create animation from selection"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("About"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("Preferences"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ Add Selection"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("Restore this revision"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("Untitled Project"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Language changes are applied immediately."));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("&Filters"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("Background Removal"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("Live Preview"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("Auto-detect Sprite Boxes"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("Color Adjustment (HSV & Contrast)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("Pixel Art Rescale..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("Retro Palette & Dithering..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("Geometry & Transform"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("Atlas Bin-Packing (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("Atlas Bin-Packing (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("Export Atlas & Animations"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("Keep Current Layout (WYSIWYG — As Displayed)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — Recommended)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("Force Power of Two (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("Packing Efficiency: Preserved as-is (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("Tight Mesh & 2D Polygon Packing"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("Approximation Tolerance (ε):"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("Apply to Selection"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — SpriteStudio"), QStringLiteral("Pixel Editor — SpriteStudio"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("Pencil (1px continuous Bresenham) [P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("Discard"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("Save"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("Apply"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("Cancel"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("Apply"));

        QCoreApplication::removeTranslator(&enTranslator);
    }

    // 3. Test Japanese translation
    {
        QTranslator jaTranslator;
        bool loaded = jaTranslator.load(QStringLiteral("sprite_studio_ja_JA.qm"), qmDir);
        QVERIFY2(loaded, "Failed to load sprite_studio_ja_JA.qm from QM_DIR");

        QCoreApplication::installTranslator(&jaTranslator);

        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILE"), QStringLiteral("ファイル"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_VIEW"), QStringLiteral("表示(&V)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOLBAR_MAIN"), QStringLiteral("メインツールバー"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_DOCK_PREVIEW"), QStringLiteral("アニメーションプレビュー"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_OPEN"), QStringLiteral("開く(&O)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_ACTION_SAVE"), QStringLiteral("保存(&S)"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_TOOL_SELECT"), QStringLiteral("選択・編集"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_CTX_CREATE_ANIM"), QStringLiteral("選択範囲からアニメーションを作成する"));
        QCOMPARE(QCoreApplication::translate("AboutDialog", "KEY_DIALOG_ABOUT_TITLE"), QStringLiteral("情報"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_TITLE"), QStringLiteral("設定"));
        QCOMPARE(QCoreApplication::translate("TimelineFilmstripWidget", "KEY_TIMELINE_ADD_SELECTION"), QStringLiteral("+ 選択を追加"));
        QCOMPARE(QCoreApplication::translate("GitHistoryDock", "KEY_GIT_BTN_RESTORE"), QStringLiteral("このリビジョンを復元"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_UNTITLED_PROJECT"), QStringLiteral("無題のプロジェクト"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("言語の変更は即座に適用されます。"));
        QCOMPARE(QCoreApplication::translate("MainWindow", "KEY_MENU_FILTERS"), QStringLiteral("フィルター(&F)"));
        QCOMPARE(QCoreApplication::translate("BackgroundRemovalDialog", "Background Removal"), QStringLiteral("背景の削除"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Live Preview"), QStringLiteral("リアルタイムプレビュー"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Auto-detect Sprite Boxes"), QStringLiteral("スプライト枠の自動検出"));
        QCOMPARE(QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)..."), QStringLiteral("カラー調整 (HSV・コントラスト)..."));
        QCOMPARE(QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale..."), QStringLiteral("ピクセルアートリサイズ..."));
        QCOMPARE(QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering..."), QStringLiteral("レトロパレット＆ディザリング..."));
        QCOMPARE(QCoreApplication::translate("FilterRegistry", "Geometry & Transform"), QStringLiteral("ジオメトリと変形"));
        QCOMPARE(QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)..."), QStringLiteral("アトラスビンパッキング (MaxRects)..."));
        QCOMPARE(QCoreApplication::translate("AtlasPackingDialog", "Atlas Bin-Packing (MaxRects)"), QStringLiteral("アトラスビンパッキング (MaxRects)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Export Atlas & Animations"), QStringLiteral("アトラスとアニメーションのエクスポート"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Keep Current Layout (WYSIWYG — As Displayed)"), QStringLiteral("現在の配置を維持 (WYSIWYG — 表示通り)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "MaxRects (Best Short Side Fit — Recommended)"), QStringLiteral("MaxRects (Best Short Side Fit — 推奨)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Force Power of Two (2^n)"), QStringLiteral("2の累乗サイズに強制 (2^n)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Packing Efficiency: Preserved as-is (WYSIWYG)"), QStringLiteral("充填効率：そのまま維持 (WYSIWYG)"));
        QCOMPARE(QCoreApplication::translate("ExportDialog", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Tight Mesh & 2D Polygon Packing"), QStringLiteral("2Dポリゴンメッシュ＆タイトパッキング"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Approximation Tolerance (ε):"), QStringLiteral("近似許容値 (ε)："));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Apply to Selection"), QStringLiteral("選択範囲に適用"));
        QCOMPARE(QCoreApplication::translate("PolygonMeshDialog", "Target Frame %1: Rectangle mode (no mesh applied)"), QStringLiteral("対象フレーム %1：矩形モード（メッシュ未適用）"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pixel Editor — SpriteStudio"), QStringLiteral("ピクセルエディタ — SpriteStudio"));
        QCOMPARE(QCoreApplication::translate("PixelEditorDialog", "Pencil (1px continuous Bresenham) [P]"), QStringLiteral("鉛筆（1px連続ブレゼンハム）[P]"));

        // Standard modal buttons (OK, Cancel, Discard, Save, Apply...)
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "OK"), QStringLiteral("OK"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Discard"), QStringLiteral("破棄"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Save"), QStringLiteral("保存"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Apply"), QStringLiteral("適用"));
        QCOMPARE(QCoreApplication::translate("FilterDialogBase", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Cancel"), QStringLiteral("キャンセル"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "Apply"), QStringLiteral("適用"));

        QCoreApplication::removeTranslator(&jaTranslator);
    }

    // 4. Test ProjectController::currentProjectName() returns translated title
    {
        SpriteDocument doc;
        QUndoStack undo;
        ProjectController pc(&doc, &undo);

        QTranslator frTranslator;
        QVERIFY(frTranslator.load(QStringLiteral("sprite_studio_fr_FR.qm"), qmDir));
        QCoreApplication::installTranslator(&frTranslator);
        QCOMPARE(pc.currentProjectName(), QStringLiteral("Projet sans titre"));
        QCoreApplication::removeTranslator(&frTranslator);

        QTranslator enTranslator;
        QVERIFY(enTranslator.load(QStringLiteral("sprite_studio_en_US.qm"), qmDir));
        QCoreApplication::installTranslator(&enTranslator);
        QCOMPARE(pc.currentProjectName(), QStringLiteral("Untitled Project"));
        QCoreApplication::removeTranslator(&enTranslator);

        QTranslator jaTranslator;
        QVERIFY(jaTranslator.load(QStringLiteral("sprite_studio_ja_JA.qm"), qmDir));
        QCoreApplication::installTranslator(&jaTranslator);
        QCOMPARE(pc.currentProjectName(), QStringLiteral("無題のプロジェクト"));
        QCoreApplication::removeTranslator(&jaTranslator);
    }

    // 5. Test Untranslated Key Fallback
    // Missing keys must clearly show visually that they are keys and not final text
    QString untranslated = QCoreApplication::translate("MainWindow", "KEY_UNKNOWN_FEATURE");
    QCOMPARE(untranslated, QStringLiteral("KEY_UNKNOWN_FEATURE"));
    QVERIFY(untranslated.startsWith(QStringLiteral("KEY_")));
#endif
}

void TestControllers::testAtlasBoxItemHandleCosmeticSize()
{
    // Test on micro-sprite (16x16) and normal sprite (64x64)
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.show();

    AtlasBoxItem item16(0, QRect(0, 0, 16, 16), QRect(0, 0, 256, 256));
    scene.addItem(&item16);
    item16.setSelectedBox(true);

    // 1. Test at 1x zoom (scale = 1.0)
    view.resetTransform();
    double size1x = item16.currentHandleSize();
    // 16 * 0.35 = 5.6, so size is capped to 5.6 to prevent overlapping handles on small sprites
    QVERIFY(size1x <= 5.6);
    QVERIFY(size1x >= 1.0);

    // 2. Test at 4x zoom (scale = 4.0)
    view.scale(4.0, 4.0);
    double size4x = item16.currentHandleSize();
    // In scene coords, size4x should be 8.0 / 4.0 = 2.0
    QCOMPARE(size4x, 2.0);

    // 3. Test at 16x zoom (scale = 16.0)
    view.resetTransform();
    view.scale(16.0, 16.0);
    double size16x = item16.currentHandleSize();
    // In scene coords, size16x should be 8.0 / 16.0 = 0.5
    QCOMPARE(size16x, 0.5);

    // 4. Test boundingRect and shape at 16x
    QRectF br = item16.boundingRect();
    QVERIFY(br.contains(item16.boxRect()));
    QPainterPath sp = item16.shape();
    QVERIFY(!sp.isEmpty());
}

void TestControllers::testAtlasViewControllerGroupDrag()
{
    QGraphicsView view;
    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Add 3 frames
    doc.addSlice(QRect(10, 10, 20, 20));
    doc.addSlice(QRect(40, 10, 20, 20));
    doc.addSlice(QRect(70, 10, 20, 20));
    QCOMPARE(doc.frameCount(), 3);

    // Select all 3 frames
    controller.setSelectedBoxIndices({0, 1, 2});
    QCOMPARE(controller.selectedBoxIndices().size(), 3);

    // Move group by (15, 25)
    controller.moveSelectedBoxes(15, 25);

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Undo
    QVERIFY(undoStack.canUndo());
    undoStack.undo();

    QCOMPARE(doc.box(0).rect, QRect(10, 10, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(40, 10, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(70, 10, 20, 20));

    // Test Redo
    QVERIFY(undoStack.canRedo());
    undoStack.redo();

    QCOMPARE(doc.box(0).rect, QRect(25, 35, 20, 20));
    QCOMPARE(doc.box(1).rect, QRect(55, 35, 20, 20));
    QCOMPARE(doc.box(2).rect, QRect(85, 35, 20, 20));

    // Test Boundary Clamping: try to move beyond atlas width (200)
    controller.moveSelectedBoxes(500, 0);
    QVERIFY(doc.box(2).rect.right() <= atlas.rect().right());
    QVERIFY(doc.box(0).rect.left() >= atlas.rect().left());
}

void TestControllers::testAtlasViewControllerContinuousSlice()
{
    QGraphicsView view;
    view.resize(400, 400);
    view.show();

    SpriteDocument doc;
    QUndoStack undoStack;
    AtlasViewController controller(&view, &doc, &undoStack);

    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    // Set ToolAddSlice mode
    controller.setToolMode(AtlasViewController::ToolAddSlice);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);

    // 1. Draw without Shift -> auto-switches to ToolSelect
    QPoint p1 = view.mapFromScene(QPointF(20, 20));
    QPoint p2 = view.mapFromScene(QPointF(80, 80));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, p1);
    QTest::mouseMove(view.viewport(), p2);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, p2);

    QCOMPARE(doc.frameCount(), 1);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolSelect);

    // 2. Draw WITH Shift -> remains in ToolAddSlice mode for rapid chaining
    controller.setToolMode(AtlasViewController::ToolAddSlice);

    QPoint p3 = view.mapFromScene(QPointF(100, 100));
    QPoint p4 = view.mapFromScene(QPointF(160, 160));

    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p3);
    QTest::mouseMove(view.viewport(), p4);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::ShiftModifier, p4);

    QCOMPARE(doc.frameCount(), 2);
    QCOMPARE(controller.toolMode(), AtlasViewController::ToolAddSlice);
}

void TestControllers::testDockStatePersistence()
{
    QSettings settings(QStringLiteral("SpriteStudioTestOrg"), QStringLiteral("SpriteStudioTestApp"));
    settings.clear();

    // 1. Setup main window with two named dock widgets
    QMainWindow mw;
    mw.setObjectName(QStringLiteral("MainWindow"));

    QDockWidget *dock1 = new QDockWidget(QStringLiteral("Preview"), &mw);
    dock1->setObjectName(QStringLiteral("dockPreview"));
    mw.addDockWidget(Qt::RightDockWidgetArea, dock1);

    QDockWidget *dock2 = new QDockWidget(QStringLiteral("Timeline"), &mw);
    dock2->setObjectName(QStringLiteral("dockTimeline"));
    mw.addDockWidget(Qt::BottomDockWidgetArea, dock2);

    mw.resize(800, 600);
    mw.show();
    dock1->show();
    dock2->show();

    // Save initial state
    QByteArray initialState = mw.saveState();
    settings.setValue(QStringLiteral("mainWindow/windowState"), initialState);
    QVERIFY(!initialState.isEmpty());

    // 2. Modify layout: hide dock1, move dock2 to top
    dock1->hide();
    mw.addDockWidget(Qt::TopDockWidgetArea, dock2);
    QVERIFY(dock1->isHidden());
    QCOMPARE(mw.dockWidgetArea(dock2), Qt::TopDockWidgetArea);

    // 3. Restore initial state from QSettings
    QByteArray restoredState = settings.value(QStringLiteral("mainWindow/windowState")).toByteArray();
    QCOMPARE(restoredState, initialState);
    bool ok = mw.restoreState(restoredState);
    QVERIFY(ok);

    // Verify dock1 is restored visible and dock2 is back at BottomDockWidgetArea
    QVERIFY(!dock1->isHidden());
    QCOMPARE(mw.dockWidgetArea(dock2), Qt::BottomDockWidgetArea);

    // Clean up test settings
    settings.clear();
}

void TestControllers::testSelectionOrderPreserved()
{
    // 1. SpriteDocument selection order
    SpriteDocument doc;
    QImage atlas(200, 200, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);

    for (int i = 0; i < 6; ++i) {
        doc.addSlice(QRect(i * 20, 0, 20, 20));
    }
    QCOMPARE(doc.frameCount(), 6);

    // Explicitly set arbitrary selection order [4, 2, 5, 1]
    QList<int> customOrder = {4, 2, 5, 1};
    doc.setSelectedFrameIndices(customOrder);
    QCOMPARE(doc.selectedFrameIndices(), customOrder);

    // Adding frame 3 appends to selection
    doc.setBoxSelection(3, true);
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 2, 5, 1, 3}));

    // Removing frame 2 preserves the order of remaining elements
    doc.setBoxSelection(2, false);
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 5, 1, 3}));

    // 2. AtlasViewController interactive click selection order
    QGraphicsView view;
    AtlasViewController atlasCtrl(&view, &doc);
    atlasCtrl.setAtlasImage(atlas);
    QCOMPARE(atlasCtrl.boxCount(), 6);

    // Initial click on frame 4
    emit atlasCtrl.boxItems()[4]->boxSelected(4, true, Qt::NoModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4}));

    // Ctrl-click on frame 2
    emit atlasCtrl.boxItems()[2]->boxSelected(2, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2}));

    // Ctrl-click on frame 5
    emit atlasCtrl.boxItems()[5]->boxSelected(5, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2, 5}));

    // Ctrl-click on frame 1
    emit atlasCtrl.boxItems()[1]->boxSelected(1, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 2, 5, 1}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 2, 5, 1}));

    // Ctrl-click to deselect frame 2
    emit atlasCtrl.boxItems()[2]->boxSelected(2, true, Qt::ControlModifier);
    QCOMPARE(atlasCtrl.selectedBoxIndices(), (QList<int>{4, 5, 1}));
    QCOMPARE(doc.selectedFrameIndices(), (QList<int>{4, 5, 1}));

    // 3. AnimationController with selection order
    AnimationPlayer player;
    AnimationController animCtrl(&doc, nullptr, &player);

    animCtrl.updateCurrentAnimation(QList<int>{4, 2, 5, 1});
    QVERIFY(doc.hasAnimation(QStringLiteral("current")));
    QCOMPARE(doc.animation(QStringLiteral("current")).frameIndices, (QList<int>{4, 2, 5, 1}));

    animCtrl.createAnimation(QStringLiteral("ordered_anim"), QList<int>{4, 2, 5, 1});
    QVERIFY(doc.hasAnimation(QStringLiteral("ordered_anim")));
    QCOMPARE(doc.animation(QStringLiteral("ordered_anim")).frameIndices, (QList<int>{4, 2, 5, 1}));
}

void TestControllers::testFilterRegistry()
{
    FilterRegistry &reg = FilterRegistry::instance();
    reg.initDefaultFilters();
    reg.loadPlugins(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins")));

    // Multiple calls to init/load should never create duplicates
    reg.initDefaultFilters();
    QCOMPARE(reg.filters().size(), 9);
    QVERIFY(reg.findFilter(QStringLiteral("background_removal")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("despill")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("outline")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("color_swap")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("color_adjust")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("pixel_rescale")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("retro_palette")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("atlas_packing")) != nullptr);
    QVERIFY(reg.findFilter(QStringLiteral("tight_polygon_packing")) != nullptr);

    QStringList cats = reg.categories();
    QVERIFY(!cats.isEmpty());

    QMenu testMenu;
    SpriteDocument doc;
    QUndoStack undoStack;
    reg.populateMenu(&testMenu, &doc, &undoStack, nullptr);
    
    // Count non-separator, non-section menu actions (the actual filter items)
    int filterActionCount = 0;
    for (QAction *act : testMenu.actions()) {
        if (!act->isSeparator() && act->menuRole() != QAction::ApplicationSpecificRole) {
            filterActionCount++;
        }
    }
    // Exactly 9 filter actions without any duplicates
    QCOMPARE(filterActionCount, 9);
}

void TestControllers::testDespillFilterAlgorithm()
{
    QImage src(20, 20, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb greenFringe = qRgb(0, 255, 0);
    QRgb redBody = qRgb(255, 0, 0);

    for (int y = 6; y <= 13; ++y) {
        for (int x = 6; x <= 13; ++x) {
            src.setPixel(x, y, redBody);
        }
    }
    for (int x = 5; x <= 14; ++x) {
        src.setPixel(x, 5, greenFringe);
        src.setPixel(x, 14, greenFringe);
    }
    for (int y = 5; y <= 14; ++y) {
        src.setPixel(5, y, greenFringe);
        src.setPixel(14, y, greenFringe);
    }

    // 1. StrictAlpha mode
    int modifiedAlpha = 0;
    QImage resAlpha = DespillFilterDialog::applyDespill(src, greenFringe, 30, DespillFilterDialog::StrictAlpha, {}, &modifiedAlpha);
    QVERIFY(modifiedAlpha > 0);
    QCOMPARE(qAlpha(resAlpha.pixel(5, 5)), 0);
    QCOMPARE(qAlpha(resAlpha.pixel(6, 6)), 255);
    QCOMPARE(resAlpha.pixel(6, 6), redBody);

    // 2. ColorClamping mode: green fringe pixel is converted to adjacent red
    int modifiedClamp = 0;
    QImage resClamp = DespillFilterDialog::applyDespill(src, greenFringe, 30, DespillFilterDialog::ColorClamping, {}, &modifiedClamp);
    QVERIFY(modifiedClamp > 0);
    QCOMPARE(qAlpha(resClamp.pixel(5, 6)), 255);
    QCOMPARE(qRed(resClamp.pixel(5, 6)), 255);
    QCOMPARE(qGreen(resClamp.pixel(5, 6)), 0);
}

void TestControllers::testOutlineFilterAlgorithm()
{
    QImage src(12, 12, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb body = qRgb(200, 50, 50);
    QRgb blackOutline = qRgb(0, 0, 0);

    for (int y = 4; y <= 7; ++y) {
        for (int x = 4; x <= 7; ++x) {
            src.setPixel(x, y, body);
        }
    }

    // 1px 4-connected outline
    QImage res = OutlineFilterDialog::applyOutline(src, 1, blackOutline, OutlineFilterDialog::FourConnected, false);

    QCOMPARE(qAlpha(res.pixel(4, 3)), 255);
    QCOMPARE(qRed(res.pixel(4, 3)), 0);
    QCOMPARE(qAlpha(res.pixel(3, 4)), 255);
    QCOMPARE(qRed(res.pixel(3, 4)), 0);
    QCOMPARE(qAlpha(res.pixel(3, 3)), 0); // corner is 0 in 4-connected
    QCOMPARE(res.pixel(5, 5), body); // interior untouched

    // Silhouette mode
    QImage resSil = OutlineFilterDialog::applyOutline(src, 1, blackOutline, OutlineFilterDialog::FourConnected, true);
    QCOMPARE(qRed(resSil.pixel(5, 5)), 0);
}

void TestControllers::testColorSwapFilterAlgorithm()
{
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    QRgb greenBase = qRgb(0, 200, 0);
    QRgb redTarget = qRgb(220, 20, 20);

    src.setPixel(3, 3, greenBase);

    int modified = 0;
    QImage res = ColorSwapFilterDialog::applyColorSwap(src, greenBase, redTarget, 20, true, {}, &modified);
    QCOMPARE(modified, 1);

    QRgb swapped = res.pixel(3, 3);
    QCOMPARE(qAlpha(swapped), 255);
    QVERIFY(qRed(swapped) > 160);
    QVERIFY(qGreen(swapped) < 60);
}

void TestControllers::testColorAdjustFilterAlgorithm()
{
    QImage src(10, 10, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));

    // Pure red pixel at (2, 2)
    src.setPixel(2, 2, qRgb(255, 0, 0));
    // Neutral gray pixel at (5, 5)
    src.setPixel(5, 5, qRgb(128, 128, 128));

    // 1. Hue Shift: +120° on pure red -> should become predominantly green
    QImage hueRes = ColorAdjustFilterDialog::applyColorAdjust(src, 120, 0, 0, 0);
    QRgb shiftedRed = hueRes.pixel(2, 2);
    QCOMPARE(qAlpha(shiftedRed), 255);
    QVERIFY(qGreen(shiftedRed) > 200);
    QVERIFY(qRed(shiftedRed) < 50);
    // Transparent pixel remains transparent
    QCOMPARE(qAlpha(hueRes.pixel(0, 0)), 0);

    // 2. Saturation: -100% on pure red -> should become grayscale (R == G == B)
    QImage desatRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, -100, 0, 0);
    QRgb grayPix = desatRes.pixel(2, 2);
    QCOMPARE(qAlpha(grayPix), 255);
    QCOMPARE(qRed(grayPix), qGreen(grayPix));
    QCOMPARE(qGreen(grayPix), qBlue(grayPix));

    // 3. Brightness/Value: -100% -> should become completely black
    QImage darkRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, 0, -100, 0);
    QRgb darkPix = darkRes.pixel(2, 2);
    QCOMPARE(qAlpha(darkPix), 255);
    QCOMPARE(qRed(darkPix), 0);
    QCOMPARE(qGreen(darkPix), 0);
    QCOMPARE(qBlue(darkPix), 0);

    // 4. Target Areas scope test: only pixels inside the rect are altered
    QList<QRect> scope = { QRect(1, 1, 3, 3) }; // covers (2,2), not (5,5)
    QImage scopedRes = ColorAdjustFilterDialog::applyColorAdjust(src, 0, 0, -100, 0, scope);
    QCOMPARE(qRed(scopedRes.pixel(2, 2)), 0); // modified
    QCOMPARE(qRed(scopedRes.pixel(5, 5)), 128); // untouched
}

void TestControllers::testPixelRescaleFilterAlgorithm()
{
    // Create a 4x4 test pattern
    QImage src(4, 4, QImage::Format_ARGB32);
    src.fill(qRgb(255, 255, 255));
    src.setPixel(1, 1, qRgb(255, 0, 0));
    src.setPixel(2, 2, qRgb(0, 0, 255));

    // 1. Nearest 2x -> 8x8 image
    QImage near2x = PixelRescaleFilterDialog::applyNearest(src, 2.0);
    QCOMPARE(near2x.width(), 8);
    QCOMPARE(near2x.height(), 8);
    QCOMPARE(near2x.pixel(2, 2), qRgb(255, 0, 0));
    QCOMPARE(near2x.pixel(3, 3), qRgb(255, 0, 0));
    QCOMPARE(near2x.pixel(4, 4), qRgb(0, 0, 255));
    QCOMPARE(near2x.pixel(0, 0), qRgb(255, 255, 255));

    // 2. Nearest 0.5x -> 2x2 image
    QImage nearHalf = PixelRescaleFilterDialog::applyNearest(src, 0.5);
    QCOMPARE(nearHalf.width(), 2);
    QCOMPARE(nearHalf.height(), 2);

    // 3. Scale2x -> 8x8 image
    QImage scale2x = PixelRescaleFilterDialog::applyScale2x(src);
    QCOMPARE(scale2x.width(), 8);
    QCOMPARE(scale2x.height(), 8);
    QVERIFY(!scale2x.isNull());

    // 4. Scale3x -> 12x12 image
    QImage scale3x = PixelRescaleFilterDialog::applyScale3x(src);
    QCOMPARE(scale3x.width(), 12);
    QCOMPARE(scale3x.height(), 12);

    // 5. Test Bounding Box proportional rescaling in dialog
    SpriteDocument doc;
    doc.setAtlas(src);
    SpriteBox b0;
    b0.rect = QRect(1, 1, 2, 2);
    b0.index = 0;
    doc.setFrames({ src.copy(b0.rect) }, { b0 });

    QUndoStack stack;
    PixelRescaleFilterDialog dlg(&doc, &stack);
    // Auto-detect disabled: bounding box is scaled mathematically
    dlg.setAutoDetectBoxesEnabled(false);
    // Dialog accepts default 2x scale
    dlg.accept();

    QCOMPARE(doc.atlas().width(), 8);
    QCOMPARE(doc.atlas().height(), 8);
    QCOMPARE(doc.boxes().size(), 1);
    QCOMPARE(doc.boxes().first().rect, QRect(2, 2, 4, 4));

    // Undo restores original atlas and box
    stack.undo();
    QCOMPARE(doc.atlas().width(), 4);
    QCOMPARE(doc.atlas().height(), 4);
    QCOMPARE(doc.boxes().first().rect, QRect(1, 1, 2, 2));
}

void TestControllers::testRetroPaletteFilterAlgorithm()
{
    // 1. Built-in Preset verification
    QVector<QRgb> dmg = RetroPaletteFilterDialog::getPresetPalette(RetroPaletteFilterDialog::GameBoyDMG);
    QCOMPARE(dmg.size(), 4);

    QVector<QRgb> pico8 = RetroPaletteFilterDialog::getPresetPalette(RetroPaletteFilterDialog::Pico8);
    QCOMPARE(pico8.size(), 16);

    QVector<QRgb> nes = RetroPaletteFilterDialog::getPresetPalette(RetroPaletteFilterDialog::NES);
    QCOMPARE(nes.size(), 54);

    QVector<QRgb> endesga = RetroPaletteFilterDialog::getPresetPalette(RetroPaletteFilterDialog::Endesga32);
    QCOMPARE(endesga.size(), 32);

    // 2. Nearest Quantization without dithering
    QImage src(4, 4, QImage::Format_ARGB32);
    src.fill(qRgba(0, 0, 0, 0));
    src.setPixel(0, 0, qRgb(0, 0, 0));       // Pure black -> Darkest DMG green (15, 56, 15)
    src.setPixel(1, 1, qRgb(255, 255, 255)); // Pure white -> Lightest DMG green (155, 188, 15)

    QImage quantNone = RetroPaletteFilterDialog::applyRetroPalette(
        src, dmg, RetroPaletteFilterDialog::DitherNone, 0);

    QCOMPARE(qAlpha(quantNone.pixel(0, 0)), 255);
    QCOMPARE(quantNone.pixel(0, 0), dmg[0]); // darkest green
    QCOMPARE(quantNone.pixel(1, 1), dmg[3]); // lightest green
    QCOMPARE(qAlpha(quantNone.pixel(2, 2)), 0); // transparent pixel untouched

    // 3. Ordered Bayer Dithering
    QImage ditherSrc(8, 8, QImage::Format_ARGB32);
    // Fill with intermediate gray to trigger spatial alternating pattern
    ditherSrc.fill(qRgb(100, 120, 50));
    QImage ditherRes = RetroPaletteFilterDialog::applyRetroPalette(
        ditherSrc, dmg, RetroPaletteFilterDialog::Bayer4x4, 50);

    // Verify dithering generated spatial variation (multiple palette tones) across uniform input
    QSet<QRgb> uniqueDitherColors;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            uniqueDitherColors.insert(ditherRes.pixel(x, y));
        }
    }
    QVERIFY(uniqueDitherColors.size() >= 2);

    // 4. Custom Palette Parser test
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString hexFile = tempDir.filePath(QStringLiteral("test_palette.hex"));
    {
        QFile f(hexFile);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "#ff0000\n";
        out << "00ff00\n";
        out << "#0000ff\n";
    }

    QString parseErr;
    QVector<QRgb> parsed = RetroPaletteFilterDialog::loadPaletteFromFile(hexFile, &parseErr);
    QVERIFY(parseErr.isEmpty());
    QCOMPARE(parsed.size(), 3);
    QCOMPARE(parsed[0], qRgb(255, 0, 0));
    QCOMPARE(parsed[1], qRgb(0, 255, 0));
    QCOMPARE(parsed[2], qRgb(0, 0, 255));
}

void TestControllers::testApplyFilterCommandUndoRedo()
{
    SpriteDocument doc;
    QImage img1(10, 10, QImage::Format_ARGB32);
    img1.fill(qRgb(255, 255, 255));
    doc.setAtlas(img1);

    SpriteBox b1;
    b1.rect = QRect(0, 0, 10, 10);
    b1.index = 0;
    doc.setFrames({ img1 }, { b1 });

    QImage img2(10, 10, QImage::Format_ARGB32);
    img2.fill(qRgb(0, 0, 255));

    QUndoStack stack;
    stack.push(new ApplyFilterCommand(&doc, QStringLiteral("Blue Filter"),
                                      img1, doc.frames(), doc.boxes(), doc.animations(),
                                      img2, { img2 }, { b1 }, doc.animations()));

    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 0);

    stack.undo();
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);

    stack.redo();
    QCOMPARE(qBlue(doc.atlas().pixel(0, 0)), 255);
    QCOMPARE(qRed(doc.atlas().pixel(0, 0)), 0);
}

void TestControllers::testFilterAutoDetectBoxes()
{
    SpriteDocument doc;
    QImage baseImg(32, 32, QImage::Format_ARGB32);
    baseImg.fill(qRgba(0, 0, 0, 0));

    // Draw an 8x8 white square in the center (from 12,12 to 19,19)
    for (int y = 12; y < 20; ++y) {
        for (int x = 12; x < 20; ++x) {
            baseImg.setPixel(x, y, qRgb(255, 255, 255));
        }
    }
    doc.setAtlas(baseImg);

    SpriteBox initialBox;
    initialBox.rect = QRect(12, 12, 8, 8);
    initialBox.index = 0;
    doc.setFrames({ baseImg.copy(initialBox.rect) }, { initialBox });

    // 1. Test OutlineFilterDialog with auto-detect enabled (default for outline)
    {
        QUndoStack stack;
        OutlineFilterDialog dlg(&doc, &stack);
        QCOMPARE(dlg.isAutoDetectBoxesEnabled(), true);

        // Accept the dialog to commit
        dlg.accept();

        QVERIFY(doc.boxes().size() >= 1);
        QRect detectedRect = doc.boxes().first().rect;
        // Outline thickness is 1 by default (or 1px expanded: 12-1=11 to 19+1=20 => 10x10)
        QCOMPARE(detectedRect, QRect(11, 11, 10, 10));
    }

    // 2. Test with auto-detect disabled
    {
        // Reset document to initial state
        doc.setAtlas(baseImg);
        doc.setFrames({ baseImg.copy(initialBox.rect) }, { initialBox });

        QUndoStack stack;
        OutlineFilterDialog dlg(&doc, &stack);
        dlg.setAutoDetectBoxesEnabled(false);
        QCOMPARE(dlg.isAutoDetectBoxesEnabled(), false);

        dlg.accept();

        // When auto-detect is disabled, the initial box rect is preserved
        QCOMPARE(doc.boxes().size(), 1);
        QCOMPARE(doc.boxes().first().rect, QRect(12, 12, 8, 8));
    }

    // 3. Test Despill, ColorSwap, ColorAdjust, PixelRescale, RetroPalette have auto-detect disabled by default
    {
        QUndoStack stack;
        DespillFilterDialog despillDlg(&doc, &stack);
        QCOMPARE(despillDlg.isAutoDetectBoxesEnabled(), false);
        despillDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(despillDlg.isAutoDetectBoxesEnabled(), true);

        ColorSwapFilterDialog swapDlg(&doc, &stack);
        QCOMPARE(swapDlg.isAutoDetectBoxesEnabled(), false);
        swapDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(swapDlg.isAutoDetectBoxesEnabled(), true);

        ColorAdjustFilterDialog adjustDlg(&doc, &stack);
        QCOMPARE(adjustDlg.isAutoDetectBoxesEnabled(), false);
        adjustDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(adjustDlg.isAutoDetectBoxesEnabled(), true);

        PixelRescaleFilterDialog rescaleDlg(&doc, &stack);
        QCOMPARE(rescaleDlg.isAutoDetectBoxesEnabled(), false);
        rescaleDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(rescaleDlg.isAutoDetectBoxesEnabled(), true);

        RetroPaletteFilterDialog retroDlg(&doc, &stack);
        QCOMPARE(retroDlg.isAutoDetectBoxesEnabled(), false);
        retroDlg.setAutoDetectBoxesEnabled(true);
        QCOMPARE(retroDlg.isAutoDetectBoxesEnabled(), true);
    }
}

void TestControllers::testAtlasPackingFilterInteractive()
{
    SpriteDocument doc;
    QUndoStack stack;

    // Create 4 frames:
    // Frame 0: 20x20 red
    // Frame 1: 30x20 green
    // Frame 2: 20x20 red (identical to frame 0)
    // Frame 3: 25x25 blue
    QImage red(20, 20, QImage::Format_ARGB32);
    red.fill(Qt::red);

    QImage green(30, 20, QImage::Format_ARGB32);
    green.fill(Qt::green);

    QImage blue(25, 25, QImage::Format_ARGB32);
    blue.fill(Qt::blue);

    QList<QImage> initialFrames = { red, green, red, blue };

    // Arrange in a naive 200x200 atlas
    QImage initialAtlas(200, 200, QImage::Format_ARGB32);
    initialAtlas.fill(Qt::transparent);

    QList<SpriteBox> initialBoxes;
    initialBoxes.reserve(4);
    for (int i = 0; i < 4; ++i) {
        SpriteBox b;
        b.rect = QRect(i * 35, 0, initialFrames[i].width(), initialFrames[i].height());
        b.index = i;
        b.pivot = QPoint(5, 5);
        b.hasCustomPivot = true;
        initialBoxes.append(b);
    }

    doc.setAtlas(initialAtlas);
    doc.setFrames(initialFrames, initialBoxes);

    // Create an animation referencing frames [0, 1, 2, 3, 2, 1]
    doc.setAnimation(QStringLiteral("walk"), {0, 1, 2, 3, 2, 1}, 12, true, SpriteAnimation::Loop);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2, 3, 2, 1}));

    // 1. Instantiate dialog with Deduplication ENABLED
    AtlasPackingDialog dlg(&doc, &stack);
    dlg.setDeduplicate(true);

    // Call accept() to apply and create undo command
    dlg.accept();

    // Verification:
    // With deduplication, unique frames count is 3 (frame 2 merged into frame 0)
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.boxes().size(), 3);

    // Animation frames must be remapped seamlessly:
    // Frame 0 -> 0
    // Frame 1 -> 1
    // Frame 2 -> 0 (merged with 0)
    // Frame 3 -> 2
    // Expected sequence: [0, 1, 0, 2, 0, 1]
    SpriteAnimation packedAnim = doc.animation(QStringLiteral("walk"));
    QCOMPARE(packedAnim.frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));
    QCOMPARE(packedAnim.fps, 12);
    QCOMPARE(packedAnim.loopMode, SpriteAnimation::Loop);

    // Bounding boxes must not overlap in the packed atlas
    for (int i = 0; i < doc.boxes().size(); ++i) {
        QVERIFY(doc.atlas().rect().contains(doc.boxes()[i].rect));
        for (int j = i + 1; j < doc.boxes().size(); ++j) {
            QVERIFY(!doc.boxes()[i].rect.intersects(doc.boxes()[j].rect));
        }
    }

    // 2. Undo test: Ctrl+Z must restore 4 frames and original animation [0, 1, 2, 3, 2, 1]
    stack.undo();
    QCOMPARE(doc.frameCount(), 4);
    QCOMPARE(doc.boxes().size(), 4);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 2, 3, 2, 1}));
    QCOMPARE(doc.atlas().size(), QSize(200, 200));

    // 3. Redo test: Ctrl+Y reapplies packing and remapped animation
    stack.redo();
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));

    // 4. Reject / Cancel test:
    AtlasPackingDialog dlgCancel(&doc, &stack);
    dlgCancel.reject();
    // Verify document remains in the 3-frame packed state without corruption
    QCOMPARE(doc.frameCount(), 3);
    QCOMPARE(doc.animation(QStringLiteral("walk")).frameIndices, (QList<int>{0, 1, 0, 2, 0, 1}));
}

void TestControllers::testAnimationControllerPivotAlignment()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);

    // Frame 0: 20x20, default pivot (10, 20)
    int idx0 = doc.addSlice(QRect(0, 0, 20, 20));
    // Frame 1: 40x60, default pivot (20, 60)
    int idx1 = doc.addSlice(QRect(20, 0, 40, 60));

    doc.setAnimation(QStringLiteral("walk"), {idx0, idx1}, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("walk"));

    // Check envelope
    QRect env = doc.computeAnimationEnvelope(QStringLiteral("walk"));
    QCOMPARE(env.x(), 20);
    QCOMPARE(env.y(), 60);
    QCOMPARE(env.width(), 40);
    QCOMPARE(env.height(), 60);

    // Seek to sequence index 0 (Frame 0)
    animCtrl.seek(0);
    QGraphicsScene *scene = animCtrl.previewScene();
    QVERIFY(scene != nullptr);
    QList<QGraphicsItem*> items = scene->items();
    QGraphicsPixmapItem *pixItem = nullptr;
    for (QGraphicsItem *it : items) {
        pixItem = dynamic_cast<QGraphicsPixmapItem*>(it);
        if (pixItem) break;
    }
    QVERIFY(pixItem != nullptr);
    // Frame 0 pos in scene: (originX - px0, originY - py0) = (20 - 10, 60 - 20) = (10, 40)
    QCOMPARE(pixItem->pos(), QPointF(10, 40));
    // Absolute pivot in scene = pos + local pivot = (20, 60)
    QCOMPARE(pixItem->pos() + doc.boxPivot(idx0), QPointF(20, 60));

    // Seek to sequence index 1 (Frame 1)
    animCtrl.seek(1);
    // Frame 1 pos in scene: (originX - px1, originY - py1) = (20 - 20, 60 - 60) = (0, 0)
    QCOMPARE(pixItem->pos(), QPointF(0, 0));
    // Absolute pivot in scene = pos + local pivot = (20, 60)
    QCOMPARE(pixItem->pos() + doc.boxPivot(idx1), QPointF(20, 60));
}

void TestControllers::testAnimationControllerReticleToggle()
{
    SpriteDocument doc;
    QImage atlas(50, 50, QImage::Format_ARGB32);
    atlas.fill(Qt::white);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 30, 30));
    doc.setAnimation(QStringLiteral("anim"), {idx}, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("anim"));

    QCOMPARE(animCtrl.showPivotReticle(), false);

    QSignalSpy spyReticle(&animCtrl, &AnimationController::showPivotReticleChanged);
    animCtrl.setShowPivotReticle(true);
    QCOMPARE(spyReticle.count(), 1);
    QCOMPARE(animCtrl.showPivotReticle(), true);

    animCtrl.setShowPivotReticle(false);
    QCOMPARE(spyReticle.count(), 2);
    QCOMPARE(animCtrl.showPivotReticle(), false);
}

void TestControllers::testAtlasBoxItemPivotDrag()
{
    QRect bounds(0, 0, 200, 200);
    AtlasBoxItem item(0, QRect(10, 10, 50, 50), bounds);

    // Initial default pivot: (25, 50)
    QCOMPARE(item.boxPivot(), QPoint(25, 50));
    QCOMPARE(item.hasCustomPivot(), false);

    // Set custom pivot
    item.setBoxPivot(QPoint(15, 30), true);
    QCOMPARE(item.boxPivot(), QPoint(15, 30));
    QCOMPARE(item.hasCustomPivot(), true);
}

void TestControllers::testAnimationPreviewZoomAndFit()
{
    SpriteDocument doc;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::blue);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 40, 40));
    doc.setAnimation(QStringLiteral("run"), {idx}, 10, true);

    QGraphicsView view;
    view.resize(300, 300);
    view.show();

    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("run"));

    QSignalSpy spyZoom(&animCtrl, &AnimationController::zoomChanged);

    // Initial zoom
    QVERIFY(animCtrl.zoomFactor() > 0.0);

    // Zoom in
    double zBefore = animCtrl.zoomFactor();
    animCtrl.zoomIn();
    QVERIFY(animCtrl.zoomFactor() > zBefore);
    QVERIFY(!spyZoom.isEmpty());

    // Zoom out
    animCtrl.zoomOut();
    QCOMPARE(animCtrl.zoomFactor(), zBefore);

    // Reset zoom
    animCtrl.resetZoom();
    QCOMPARE(animCtrl.zoomFactor(), 1.0);

    // Fit in view
    animCtrl.fitInView();
    QVERIFY(animCtrl.zoomFactor() > 0.0);
}

void TestControllers::testAnimationPreviewPivotInteraction()
{
    SpriteDocument doc;
    QUndoStack undoStack;
    QImage atlas(100, 100, QImage::Format_ARGB32);
    atlas.fill(Qt::yellow);
    doc.setAtlas(atlas);
    int idx = doc.addSlice(QRect(0, 0, 50, 50));
    doc.setAnimation(QStringLiteral("idle"), {idx}, 8, true);

    QGraphicsView view;
    view.resize(400, 400);
    view.show();

    AnimationController animCtrl(&doc, &undoStack, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("idle"));
    animCtrl.setShowPivotReticle(true);

    // Default pivot is (25, 50)
    QCOMPARE(doc.boxPivot(idx), QPoint(25, 50));

    QSignalSpy spyDrag(&animCtrl, &AnimationController::pivotDragged);
    QSignalSpy spyFinish(&animCtrl, &AnimationController::pivotDragFinished);

    // Simulate Shift+Click at position (150, 150) in viewport to place pivot directly
    QPoint clickPos(150, 150);

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(clickPos), QPointF(clickPos), QPointF(clickPos), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QApplication::sendEvent(view.viewport(), &pressEvent);

    QCOMPARE(spyDrag.count(), 1);

    // Release mouse
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(clickPos), QPointF(clickPos), QPointF(clickPos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &releaseEvent);

    QCOMPARE(spyFinish.count(), 1);
    QVERIFY(doc.box(idx).hasCustomPivot);

    // Undo restores default pivot
    QVERIFY(undoStack.canUndo());
    undoStack.undo();
    QCOMPARE(doc.box(idx).hasCustomPivot, false);
    QCOMPARE(doc.boxPivot(idx), QPoint(25, 50));

    // Redo restores dragged pivot
    QVERIFY(undoStack.canRedo());
    undoStack.redo();
    QCOMPARE(doc.box(idx).hasCustomPivot, true);
}

void TestControllers::testAnimationPolygonMasking()
{
    SpriteDocument doc;
    QImage img(40, 40, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.fillRect(0, 0, 20, 20, QColor(0, 255, 0, 255)); // Sprite content
        p.fillRect(30, 30, 10, 10, QColor(255, 0, 0, 255)); // Neighbor trace
    }

    SpriteBox box(QRect(0, 0, 40, 40));
    box.hasPolygonMesh = true;
    box.polygon << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 20) << QPointF(0, 20);
    box.triangles = { 0, 1, 2, 0, 2, 3 };
    doc.addFrame(img, box);

    doc.setAnimation(QStringLiteral("walk"), { 0 }, 12, true);

    QGraphicsView view;
    AnimationController animCtrl(&doc, nullptr, nullptr, nullptr, &view);
    animCtrl.selectAnimation(QStringLiteral("walk"));
    animCtrl.seek(0);

    QGraphicsScene *scene = animCtrl.previewScene();
    QVERIFY(scene != nullptr);

    QGraphicsPixmapItem *pixItem = nullptr;
    for (QGraphicsItem *it : scene->items()) {
        pixItem = dynamic_cast<QGraphicsPixmapItem*>(it);
        if (pixItem) break;
    }
    QVERIFY(pixItem != nullptr);

    QImage rendered = pixItem->pixmap().toImage();
    // Sprite pixels inside polygon are preserved
    QCOMPARE(rendered.pixelColor(5, 5), QColor(0, 255, 0, 255));
    // Neighbor trace outside polygon is transparent (alpha == 0)
    QCOMPARE(rendered.pixelColor(35, 35).alpha(), 0);
}

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestControllers tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controllers.moc"
