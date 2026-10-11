#include <QtTest/QtTest>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QAction>
#include <QMenuBar>
#include <QListWidget>
#include <QToolButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QDockWidget>

#include "mainwindow.h"
#include "widgets/exportdialog.h"
#include "widgets/branchselectiondialog.h"
#include "model/spritedocument.h"
#include "project/sessionmanager.h"

class TestGuiIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // MainWindow Lifecycle & Controllers
    void testMainWindowInitialization();
    void testMainWindowTitleAndDirtyFlag();
    void testMainWindowDragAndDropMimeTypes();
    void testMainWindowPlaybackTriggers();
    void testMainWindowLayersDockAndSynchronization();

    // ExportDialog Functional Tests
    void testExportDialogOptionsAndDefaults();
    void testDocumentLayerVisibilityRecompositionAndExportStats();

    // BranchSelectionDialog Functional Tests
    void testBranchSelectionDialog();
};

void TestGuiIntegration::initTestCase()
{
}

void TestGuiIntegration::cleanupTestCase()
{
}

void TestGuiIntegration::testMainWindowInitialization()
{
    MainWindow mw;
    mw.resize(1024, 768);

    // Verify specialized controllers are correctly instantiated and wired
    QVERIFY(mw.projectController() != nullptr);
    QVERIFY(mw.atlasController() != nullptr);
    QVERIFY(mw.animationController() != nullptr);

    // Verify main menu bar structure
    QMenuBar *menuBar = mw.menuBar();
    QVERIFY(menuBar != nullptr);
    QVERIFY(menuBar->actions().size() >= 3);

    // Initial window title
    QVERIFY(mw.windowTitle().contains(QStringLiteral("BentoPack")));
}

void TestGuiIntegration::testMainWindowTitleAndDirtyFlag()
{
    MainWindow mw;
    ProjectController *pc = mw.projectController();
    QVERIFY(pc != nullptr);

    // Initial state: not modified
    QVERIFY(!mw.isWindowModified());

    // Window modification flag updates
    mw.setWindowModified(true);
    QVERIFY(mw.isWindowModified());

    mw.setWindowModified(false);
    QVERIFY(!mw.isWindowModified());
}

void TestGuiIntegration::testMainWindowDragAndDropMimeTypes()
{
    MainWindow mw;

    // 1. Valid .bento file drag
    {
        QMimeData mime;
        QList<QUrl> urls;
        urls.append(QUrl::fromLocalFile(QStringLiteral("/path/to/game.bento")));
        mime.setUrls(urls);

        QDragEnterEvent event(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &event);
        QVERIFY(event.isAccepted());
    }

    // 2. Valid image (.png) drag
    {
        QMimeData mime;
        QList<QUrl> urls;
        urls.append(QUrl::fromLocalFile(QStringLiteral("/path/to/spritesheet.png")));
        mime.setUrls(urls);

        QDragEnterEvent event(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &event);
        QVERIFY(event.isAccepted());

        QDragMoveEvent moveEvent(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &moveEvent);
        QVERIFY(moveEvent.isAccepted());

        // Also test directly on centralWidget to ensure event filter intercepts for child widgets
        if (mw.centralWidget()) {
            QDragEnterEvent childEnter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(mw.centralWidget(), &childEnter);
            QVERIFY(childEnter.isAccepted());

            QDragMoveEvent childMove(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(mw.centralWidget(), &childMove);
            QVERIFY(childMove.isAccepted());
        }
    }

    // 3. Raw GNOME text/uri-list format
    {
        QMimeData mime;
        mime.setData(QStringLiteral("text/uri-list"), "file:///path/to/hero.png\r\n");

        QDragEnterEvent event(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &event);
        QVERIFY(event.isAccepted());

        QDragMoveEvent moveEvent(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &moveEvent);
        QVERIFY(moveEvent.isAccepted());
    }

    // 4. Non-URL mime data (plain text) -> rejected
    {
        QMimeData mime;
        mime.setText(QStringLiteral("Some plain text without urls"));

        QDragEnterEvent event(QPoint(100, 100), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&mw, &event);
        QVERIFY(!event.isAccepted());
    }
}

void TestGuiIntegration::testMainWindowPlaybackTriggers()
{
    MainWindow mw;
    AnimationController *ac = mw.animationController();
    QVERIFY(ac != nullptr);

    // Initial state: not playing
    QVERIFY(!ac->isPlaying());

    // Configure animation sequence on the player
    ac->player()->setSequence({0, 1}, 12);

    // Play action
    ac->play();
    QVERIFY(ac->isPlaying());

    // Pause action
    ac->pause();
    QVERIFY(!ac->isPlaying());

    // Toggle
    ac->togglePlayPause();
    QVERIFY(ac->isPlaying());
    ac->togglePlayPause();
    ac->pause();
    QVERIFY(!ac->isPlaying());

    // Transport buttons step test (verifies single-step without double-connection jump)
    QToolButton *btnNext = mw.findChild<QToolButton*>(QStringLiteral("btnNextFrame"));
    QToolButton *btnPrev = mw.findChild<QToolButton*>(QStringLiteral("btnPrevFrame"));
    QToolButton *btnFirst = mw.findChild<QToolButton*>(QStringLiteral("btnFirstFrame"));
    QToolButton *btnLast = mw.findChild<QToolButton*>(QStringLiteral("btnLastFrame"));
    QVERIFY(btnNext != nullptr);
    QVERIFY(btnPrev != nullptr);
    QVERIFY(btnFirst != nullptr);
    QVERIFY(btnLast != nullptr);

    ac->player()->setSequence({0, 1, 2, 3}, 12);
    ac->player()->seek(0);
    QCOMPARE(ac->player()->currentSequenceIndex(), 0);

    // Clicking btnNextFrame must advance by EXACTLY 1 frame (not 2)
    btnNext->click();
    QCOMPARE(ac->player()->currentSequenceIndex(), 1);

    btnNext->click();
    QCOMPARE(ac->player()->currentSequenceIndex(), 2);

    // Clicking btnPrevFrame must retreat by EXACTLY 1 frame (not 2)
    btnPrev->click();
    QCOMPARE(ac->player()->currentSequenceIndex(), 1);

    btnLast->click();
    QCOMPARE(ac->player()->currentSequenceIndex(), 3);

    btnFirst->click();
    QCOMPARE(ac->player()->currentSequenceIndex(), 0);
}

void TestGuiIntegration::testExportDialogOptionsAndDefaults()
{
    SpriteDocument doc;
    QImage atlas(128, 128, QImage::Format_ARGB32);
    atlas.fill(Qt::transparent);
    doc.setAtlas(atlas);
    doc.addSlice(QRect(0, 0, 32, 32));
    doc.addSlice(QRect(32, 0, 32, 32));
    doc.addAnimation(QStringLiteral("idle"), {0, 1}, 10);

    ExportDialog dlg(&doc, QStringLiteral("/tmp/test_export.png"));

    // Verify initial default options
    ExportOptions opts = dlg.exportOptions();
    QCOMPARE(opts.packOptions.powerOfTwo, false);
    QCOMPARE(opts.packOptions.extrude, 0);

    // File path reflection
    QVERIFY(dlg.exportFilePath().contains(QStringLiteral("test_export")));

    // Verify contextual group visibility
    auto *comboFormat = dlg.findChild<QComboBox*>(QStringLiteral("comboFormat"));
    auto *grpGeometry = dlg.findChild<QGroupBox*>(QStringLiteral("grpGeometry"));
    auto *grpVram = dlg.findChild<QGroupBox*>(QStringLiteral("grpVram"));
    auto *grpStats = dlg.findChild<QGroupBox*>(QStringLiteral("grpStats"));
    auto *grpGif = dlg.findChild<QGroupBox*>(QStringLiteral("grpGifOptions"));
    auto *grpAse = dlg.findChild<QGroupBox*>(QStringLiteral("grpAsepriteOptions"));
    auto *txtOutputDir = dlg.findChild<QLineEdit*>(QStringLiteral("txtOutputDir"));
    auto *txtBaseName = dlg.findChild<QLineEdit*>(QStringLiteral("txtBaseName"));

    QVERIFY(comboFormat != nullptr);
    QVERIFY(grpGeometry != nullptr);
    QVERIFY(grpVram != nullptr);
    QVERIFY(grpStats != nullptr);
    QVERIFY(grpGif != nullptr);
    QVERIFY(grpAse != nullptr);
    QVERIFY(txtOutputDir != nullptr);
    QVERIFY(txtBaseName != nullptr);

    // Initial state: for atlas formats, atlas/VRAM/stats are visible, GIF & Aseprite are hidden
    int gifIdx = comboFormat->findData(QStringLiteral("gif_extractor"));
    int aseIdx = comboFormat->findData(QStringLiteral("aseprite_extractor"));
    int godotIdx = comboFormat->findData(QStringLiteral("godot_extractor"));

    dlg.show();

    if (godotIdx >= 0) {
        comboFormat->setCurrentIndex(godotIdx);
        QVERIFY(!grpGeometry->isHidden());
        QVERIFY(!grpVram->isHidden());
        QVERIFY(!grpStats->isHidden());
        QVERIFY(grpGif->isHidden());
        QVERIFY(grpAse->isHidden());
    }

    // Switch to Animated GIF format
    if (gifIdx >= 0) {
        comboFormat->setCurrentIndex(gifIdx);
        QVERIFY(grpGeometry->isHidden());
        QVERIFY(grpVram->isHidden());
        QVERIFY(grpStats->isHidden());
        QVERIFY(!grpGif->isHidden());
        QVERIFY(grpAse->isHidden());
        QVERIFY(dlg.exportFilePath().endsWith(QStringLiteral(".gif")));
    }

    // Switch to Native Aseprite format
    if (aseIdx >= 0) {
        comboFormat->setCurrentIndex(aseIdx);
        QVERIFY(grpGeometry->isHidden());
        QVERIFY(grpVram->isHidden());
        QVERIFY(grpStats->isHidden());
        QVERIFY(grpGif->isHidden());
        QVERIFY(!grpAse->isHidden());
        QVERIFY(dlg.exportFilePath().endsWith(QStringLiteral(".ase")) || dlg.exportFilePath().endsWith(QStringLiteral(".aseprite")));
    }

    // Test destination directory and project name updates
    txtOutputDir->setText(QStringLiteral("C:/output_test"));
    txtBaseName->setText(QStringLiteral("my_character"));
    QVERIFY(dlg.exportFilePath().contains(QStringLiteral("my_character")));
    QVERIFY(dlg.exportFilePath().contains(QStringLiteral("output_test")));

    // Test default path and base name inferred from document's filePath
    {
        SpriteDocument fileDoc;
        fileDoc.setFilePath(QStringLiteral("/home/sample_user/sprites/warrior.aseprite"));
        ExportDialog defaultDlg(&fileDoc);
        auto *outDir = defaultDlg.findChild<QLineEdit*>(QStringLiteral("txtOutputDir"));
        auto *baseName = defaultDlg.findChild<QLineEdit*>(QStringLiteral("txtBaseName"));
        QVERIFY(outDir != nullptr);
        QVERIFY(baseName != nullptr);
        QCOMPARE(outDir->text(), QDir::toNativeSeparators(QStringLiteral("/home/sample_user/sprites")));
        QCOMPARE(baseName->text(), QStringLiteral("warrior"));
        QVERIFY(defaultDlg.exportFilePath().startsWith(QDir::toNativeSeparators(QStringLiteral("/home/sample_user/sprites"))));
        QVERIFY(defaultDlg.exportFilePath().contains(QStringLiteral("warrior")));
    }
}

void TestGuiIntegration::testBranchSelectionDialog()
{
    QList<GitCommitInfo> branches;

    GitCommitInfo b1;
    b1.hash = QStringLiteral("commit_hash_alpha_123");
    b1.message = QStringLiteral("Slice modification branch");
    b1.timestamp = QDateTime::currentDateTime();
    branches.append(b1);

    GitCommitInfo b2;
    b2.hash = QStringLiteral("commit_hash_beta_456");
    b2.message = QStringLiteral("Background removal branch");
    b2.timestamp = QDateTime::currentDateTime().addSecs(60);
    branches.append(b2);

    BranchSelectionDialog dlg(branches);

    QListWidget *listWidget = dlg.findChild<QListWidget*>();
    QVERIFY(listWidget != nullptr);
    QCOMPARE(listWidget->count(), 2);

    // Select second branch item
    listWidget->setCurrentRow(1);
    dlg.accept();

    QCOMPARE(dlg.selectedCommitHash(), QStringLiteral("commit_hash_beta_456"));
}

void TestGuiIntegration::testMainWindowLayersDockAndSynchronization()
{
    MainWindow mw;
    mw.resize(1024, 768);

    QVERIFY(mw.layersDock() != nullptr);
    QCOMPARE(mw.layersDock()->objectName(), QStringLiteral("dockLayers"));
    QVERIFY(mw.layerStackWidget() != nullptr);
    QVERIFY(mw.layerStackWidget()->isDocumentMode());
    QVERIFY(mw.layerStackWidget()->listWidget() != nullptr);
    QVERIFY(mw.layersDock()->toggleViewAction() != nullptr);
}

void TestGuiIntegration::testDocumentLayerVisibilityRecompositionAndExportStats()
{
    SpriteDocument doc;

    SpriteLayer lyrBg;
    lyrBg.id = QStringLiteral("bg_layer");
    lyrBg.name = QStringLiteral("Background");
    lyrBg.visible = true;
    lyrBg.zOrder = 0;

    SpriteLayer lyrHero;
    lyrHero.id = QStringLiteral("hero_layer");
    lyrHero.name = QStringLiteral("Hero");
    lyrHero.visible = true;
    lyrHero.zOrder = 1;

    doc.setLayers({lyrBg, lyrHero});
    QCOMPARE(doc.layerCount(), 2);

    // Frame 0: 32x32
    QImage bgImg(32, 32, QImage::Format_ARGB32_Premultiplied);
    bgImg.fill(QColor(255, 0, 0, 255)); // pure red background

    QImage heroImg(32, 32, QImage::Format_ARGB32_Premultiplied);
    heroImg.fill(Qt::transparent);
    // Draw blue pixel at (16, 16) only
    heroImg.setPixelColor(16, 16, QColor(0, 0, 255, 255));

    SpriteBox box(QRect(0, 0, 32, 32));
    doc.addFrame(QImage(32, 32, QImage::Format_ARGB32_Premultiplied), box);

    SpriteCel celBg;
    celBg.layerIndex = 0;
    celBg.layerId = lyrBg.id;
    celBg.image = bgImg;

    SpriteCel celHero;
    celHero.layerIndex = 1;
    celHero.layerId = lyrHero.id;
    celHero.image = heroImg;

    doc.setFrameCels(0, {celBg, celHero});

    // Initial recomposition
    doc.recompositeAllFrames();
    doc.recompositeAtlas();

    // Check pixel at (0, 0) is red, and (16, 16) is blue
    QImage comp1 = doc.frame(0);
    QCOMPARE(comp1.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(comp1.pixelColor(16, 16), QColor(0, 0, 255, 255));

    // Verify ExportDialog stats shows 2/2 visible
    // Verify ExportDialog stats shows 2/2 visible
    {
        ExportDialog exportDlg(&doc);
        auto *comboFormat = exportDlg.findChild<QComboBox*>("comboFormat");
        int godotIdx = comboFormat ? comboFormat->findData(QStringLiteral("godot_extractor")) : -1;
        if (godotIdx >= 0) {
            comboFormat->setCurrentIndex(godotIdx);
        }
        exportDlg.show();
        QTest::qWait(100);
        QLabel *lblFrames = exportDlg.findChild<QLabel*>("lblFrames");
        QVERIFY(lblFrames != nullptr);
        QVERIFY(lblFrames->text().contains(QStringLiteral("Layers: 2/2 visible")));
    }

    // Now hide background layer
    doc.setLayerVisible(0, false);
    QVERIFY(!doc.layer(0).visible);

    doc.recompositeAllFrames();
    doc.recompositeAtlas();

    // Check pixel at (0, 0) is now completely transparent!
    QImage comp2 = doc.frame(0);
    QCOMPARE(comp2.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(comp2.pixelColor(16, 16), QColor(0, 0, 255, 255));

    // Verify ExportDialog stats now shows 1/2 visible
    {
        ExportDialog exportDlg(&doc);
        auto *comboFormat = exportDlg.findChild<QComboBox*>("comboFormat");
        int godotIdx = comboFormat ? comboFormat->findData(QStringLiteral("godot_extractor")) : -1;
        if (godotIdx >= 0) {
            comboFormat->setCurrentIndex(godotIdx);
        }
        exportDlg.show();
        QTest::qWait(100);
        QLabel *lblFrames = exportDlg.findChild<QLabel*>("lblFrames");
        QVERIFY(lblFrames != nullptr);
        QVERIFY(lblFrames->text().contains(QStringLiteral("Layers: 1/2 visible")));
    }

    // Test lock, opacity, blendMode and moveLayer
    doc.setLayerLocked(0, true);
    QVERIFY(doc.layer(0).locked);

    doc.setLayerOpacity(1, 128);
    QCOMPARE(doc.layer(1).opacity, 128);

    doc.setLayerBlendMode(1, QPainter::CompositionMode_Multiply);
    QCOMPARE(doc.layer(1).blendMode, QPainter::CompositionMode_Multiply);

    doc.setLayerName(0, QStringLiteral("NewBackgroundName"));
    QCOMPARE(doc.layer(0).name, QStringLiteral("NewBackgroundName"));

    doc.moveLayer(0, 1);
    QCOMPARE(doc.layer(0).name, QStringLiteral("Hero"));
    QCOMPARE(doc.layer(1).name, QStringLiteral("NewBackgroundName"));
}

int main(int argc, char *argv[])
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");

    QApplication app(argc, argv);
    TestGuiIntegration t;

    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromLocal8Bit(argv[i]);
    }
    if (!args.contains(QStringLiteral("-o"))) {
        args << QStringLiteral("-o") << QStringLiteral("-,txt");
    }

    int result = QTest::qExec(&t, args);
    std::fflush(stdout);
    std::fflush(stderr);
    return result;
}

#include "test_gui_integration.moc"
