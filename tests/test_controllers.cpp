#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QGraphicsView>
#include <QUndoStack>
#include <QContextMenuEvent>
#include <QTranslator>
#include <QSpinBox>
#include <QMainWindow>
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
    void testProjectControllerOpenAsync();
    void testProjectControllerRemoveBgAsync();
    void testUndoStackLimitAndImageStorage();

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

    // Wait for the async worker thread and main-thread finish
    QVERIFY(spyLoaded.wait(5000));
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

    QVERIFY(spyBg.wait(5000));
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
        QCOMPARE(QCoreApplication::translate("ProjectController", "KEY_UNTITLED_PROJECT"), QStringLiteral("Projet sans titre"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Les modifications de langue s'appliquent immédiatement."));

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
        QCOMPARE(QCoreApplication::translate("ProjectController", "KEY_UNTITLED_PROJECT"), QStringLiteral("Untitled Project"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("Language changes are applied immediately."));

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
        QCOMPARE(QCoreApplication::translate("ProjectController", "KEY_UNTITLED_PROJECT"), QStringLiteral("無題のプロジェクト"));
        QCOMPARE(QCoreApplication::translate("SettingsDialog", "KEY_SETTINGS_LANG_HINT"), QStringLiteral("言語の変更は即座に適用されます。"));

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

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestControllers tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "test_controllers.moc"
