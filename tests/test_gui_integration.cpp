#include <QtTest/QtTest>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QAction>
#include <QMenuBar>
#include <QListWidget>

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

    // ExportDialog Functional Tests
    void testExportDialogOptionsAndDefaults();

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
    }

    // 3. Non-URL mime data (plain text) -> rejected
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
    QVERIFY(!ac->isPlaying());
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
