#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "include/commands/commands.h"
#include "include/extractor/extractorregistry.h"
#include "include/config/appconfig.h"
#include "include/project/sessionmanager.h"
#include "include/widgets/githistorydock.h"
#include "include/widgets/settingsdialog.h"
#include <QShortcut>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , frameModel(new ArrangementModel(this))
    , listDelegate(new FrameDelegate(this))
    , m_document(new SpriteDocument(this))
    , m_undoStack(new QUndoStack(this))
    , m_player(new AnimationPlayer(this))
{
    ui->setupUi(this);
    setAcceptDrops(true);

    // Initialize configuration and extractor registry
    AppConfig::instance();
    ExtractorRegistry::instance();

    setupControllers();
    setupUIConnections();
    setupShortcuts();
    setupGitHistoryDock();

    // Populate recent files and recent projects menus
    updateRecentFilesMenu();
    updateRecentProjectsMenu();
    updateWindowTitle();

    // Check for crash recovery / orphan sessions
    QTimer::singleShot(100, this, &MainWindow::checkCrashRecovery);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupControllers()
{
    if (m_undoStack) {
        m_undoStack->setUndoLimit(AppConfig::instance().project().undoLimit);
    }

    m_projectController = std::make_unique<ProjectController>(m_document, m_undoStack, this);
    m_atlasController = std::make_unique<AtlasViewController>(ui->graphicsViewLayers, m_document, m_undoStack, this);
    m_animationController = std::make_unique<AnimationController>(m_document, m_undoStack, m_player,
                                                                 ui->animationList, ui->graphicsViewResult, this);

    // Connect ProjectController
    connect(m_projectController.get(), &ProjectController::fileLoaded, this, [this](const QString &filePath) {
        updateWindowTitle();
        statusLabel->setText(filePath);
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
    });

    connect(m_projectController.get(), &ProjectController::projectLoaded, this, [this](const QString &/*sspPath*/) {
        updateWindowTitle();
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
    });

    connect(m_document, &SpriteDocument::documentReset, this, [this]() {
        updateWindowTitle();
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
    });

    connect(m_projectController.get(), &ProjectController::projectSaved, this, [this](const QString &/*sspPath*/) {
        updateWindowTitle();
    });

    connect(m_projectController.get(), &ProjectController::projectModifiedChanged, this, [this](bool /*modified*/) {
        updateWindowTitle();
    });

    connect(m_projectController.get(), &ProjectController::recentProjectsChanged, this, [this]() {
        updateRecentProjectsMenu();
    });

    connect(m_projectController.get(), &ProjectController::fileLoadError, this, [this](const QString &/*path*/, const QString &err) {
        QMessageBox::critical(this, tr("KEY_MSG_LOAD_ERROR"), err);
    });

    connect(m_projectController.get(), &ProjectController::statusMessage, this, [this](const QString &msg) {
        if (statusLabel) statusLabel->setText(msg);
    });

    connect(m_projectController.get(), &ProjectController::progressChanged, this, [this](int percent) {
        if (progressBar) progressBar->setValue(percent);
    });

    connect(m_projectController.get(), &ProjectController::recentFilesChanged, this, [this]() {
        updateRecentFilesMenu();
    });

    connect(m_projectController.get(), &ProjectController::backgroundRemoved, this, [this]() {
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
    });

    connect(m_projectController.get(), &ProjectController::processingStarted, this, [this]() {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    });

    connect(m_projectController.get(), &ProjectController::processingFinished, this, [this]() {
        QApplication::restoreOverrideCursor();
    });

    // Connect AtlasViewController
    connect(m_atlasController.get(), &AtlasViewController::selectionChanged, this, [this](const QList<int> &indices) {
        if (m_isSyncingSelection) return;
        m_isSyncingSelection = true;
        // Sync frame list selection
        QItemSelection selection;
        for (int row : indices) {
            QModelIndex mIndex = frameModel->index(row, 0);
            if (mIndex.isValid()) {
                selection.select(mIndex, mIndex);
            }
        }
        ui->framesList->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        refreshFrameListDisplay();
        m_isSyncingSelection = false;

        // Update transient current animation
        m_animationController->updateCurrentAnimation(indices);
    });

    connect(m_atlasController.get(), &AtlasViewController::zoomChanged, this, [this](double zoomFactor) {
        if (zoomSlider) {
            zoomSlider->blockSignals(true);
            zoomSlider->setValue(static_cast<int>(zoomFactor * 100.0));
            zoomSlider->blockSignals(false);
        }
        if (zoomLabel) {
            zoomLabel->setText(QString::number(static_cast<int>(zoomFactor * 100.0)) + "%");
        }
    });

    connect(m_atlasController.get(), &AtlasViewController::toolModeChanged, this, [this](SliceToolMode mode) {
        ui->btnToolSelect->setChecked(mode == AtlasViewController::ToolSelect);
        ui->btnToolAddSlice->setChecked(mode == AtlasViewController::ToolAddSlice);
    });

    connect(m_atlasController.get(), &AtlasViewController::boxContextMenuRequested,
            this, &MainWindow::onBoxContextMenuRequested);

    connect(m_atlasController.get(), &AtlasViewController::atlasContextMenuRequested,
            this, &MainWindow::onAtlasContextMenuRequested);

    // Connect AnimationController
    connect(m_animationController.get(), &AnimationController::playbackStateChanged, this, [this](bool playing) {
        ui->Play->setVisible(!playing);
        ui->Pause->setVisible(playing);
    });

    connect(m_animationController.get(), &AnimationController::fpsChanged, this, [this](int fps) {
        ui->fps->blockSignals(true);
        ui->fps->setValue(fps);
        ui->fps->blockSignals(false);
        ui->timingLabel->setText(" -> " + tr("_timing") + ": " +
                                 QString::number(1000.0 / static_cast<double>(fps), 'g', 4) + "ms");
    });

    connect(m_animationController.get(), &AnimationController::framesSelectedInAnimation,
            this, [this](const QList<int> &indices) {
        if (m_atlasController && m_atlasController->selectedBoxIndices() != indices) {
            m_atlasController->setSelectedBoxIndices(indices);
        }
    });

    // Connect Document frame updates to UI model
    connect(m_document, &SpriteDocument::framesChanged, this, [this]() {
        populateFrameList(m_document->frames(), m_document->boxes());
        if (m_animationController) {
            m_animationController->updateCurrentAnimation(m_document->selectedFrameIndices());
        }
    });

    connect(m_document, &SpriteDocument::frameUpdated, this, [this](int index) {
        if (frameModel && index >= 0 && index < frameModel->rowCount()) {
            QStandardItem *it = frameModel->item(index);
            if (it) {
                QPixmap pixmap = m_document->frame(index);
                QPixmap thumbnail = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                it->setData(thumbnail, Qt::DecorationRole);
            }
        }
    });
}

void MainWindow::setupUIConnections()
{
    // Status Bar widgets
    statusLabel = new QLabel(this);
    statusLabel->setText(tr("KEY_STATUS_READY_TO_START"));
    ui->statusBar->addPermanentWidget(statusLabel, 1);

    zoomSlider = new QSlider(Qt::Horizontal, this);
    zoomSlider->setRange(10, 1000);
    zoomSlider->setValue(100);
    zoomSlider->setMinimumWidth(200);
    zoomSlider->setTickInterval(10);
    ui->statusBar->addPermanentWidget(zoomSlider);

    zoomLabel = new QLabel(this);
    zoomLabel->setText(QString::number(zoomSlider->value()) + "%");
    ui->statusBar->addPermanentWidget(zoomLabel);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFormat(tr("KEY_STATUS_PROGRESS") + " %p%");
    progressBar->setMinimumWidth(300);
    ui->statusBar->addPermanentWidget(progressBar);

    connect(zoomSlider, &QSlider::valueChanged, this, &MainWindow::zoomSliderChanged);

    // Frame list setup
    ui->framesList->setModel(frameModel);
    ui->framesList->setViewMode(QListView::IconMode);
    ui->framesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->framesList->setItemDelegate(listDelegate);
    ui->framesList->installEventFilter(this);
    ui->framesList->viewport()->installEventFilter(this);
    ui->framesList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->framesList->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        if (m_isSyncingSelection || !m_atlasController || !m_document) return;
        m_isSyncingSelection = true;
        QModelIndexList selectedRows = ui->framesList->selectionModel()->selectedRows();
        QList<int> indices;
        indices.reserve(selectedRows.size());
        for (const QModelIndex &idx : selectedRows) {
            indices.append(idx.row());
        }
        std::sort(indices.begin(), indices.end());
        if (m_atlasController->selectedBoxIndices() != indices) {
            m_atlasController->setSelectedBoxIndices(indices);
        }
        refreshFrameListDisplay();
        if (m_animationController) {
            m_animationController->updateCurrentAnimation(indices);
        }
        m_isSyncingSelection = false;
    });
    connect(frameModel, &ArrangementModel::mergeRequested, this, &MainWindow::onMergeFrames);
    connect(ui->framesList, &QListView::customContextMenuRequested,
            this, &MainWindow::on_framesList_customContextMenuRequested);

    // Slice tool buttons
    ui->btnToolSelect->setIcon(QIcon(":/drawer/icons/tool_select.png"));
    ui->btnToolSelect->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->btnToolAddSlice->setIcon(QIcon(":/drawer/icons/tool_slice.png"));
    ui->btnToolAddSlice->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->btnTrimSlice->setIcon(QIcon(":/drawer/icons/tool_trim.png"));
    ui->btnRemoveBg->setIcon(QIcon(":/drawer/icons/tool_remove_bg.png"));

    connect(ui->btnToolSelect, &QToolButton::clicked, this, &MainWindow::on_btnToolSelect_clicked);
    connect(ui->btnToolAddSlice, &QToolButton::clicked, this, &MainWindow::on_btnToolAddSlice_clicked);
    connect(ui->btnTrimSlice, &QPushButton::clicked, this, &MainWindow::on_btnTrimSlice_clicked);
    connect(ui->btnRemoveBg, &QPushButton::clicked, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    // Animation list context menu
    ui->animationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->animationList, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::on_animationList_customContextMenuRequested);

    // Initial timing label
    ui->timingLabel->setText(" -> " + tr("KEY_LABEL_TIMING") + ": " +
                             QString::number(1000.0 / static_cast<double>(ui->fps->value()), 'g', 4) + "ms");
}

void MainWindow::setupShortcuts()
{
    // Create Edit Menu for Undo/Redo
    QMenu *editMenu = new QMenu(tr("KEY_MENU_EDIT"), this);
    menuBar()->insertMenu(ui->menuHelp->menuAction(), editMenu);
    QAction *undoAction = m_undoStack->createUndoAction(this, tr("KEY_ACTION_UNDO"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);

    QAction *redoAction = m_undoStack->createRedoAction(this, tr("KEY_ACTION_REDO"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);

    editMenu->addSeparator();
    QAction *removeBgAction = editMenu->addAction(tr("KEY_ACTION_REMOVE_BG"));
    removeBgAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    editMenu->addSeparator();
    QAction *prefAction = editMenu->addAction(tr("Préférences..."));
    prefAction->setShortcut(QKeySequence::Preferences);
    connect(prefAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    ui->menuHelp->addSeparator();
    QAction *helpPrefAction = ui->menuHelp->addAction(tr("Préférences..."));
    connect(helpPrefAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    // Standard Project & File Shortcuts
    ui->actionNewProject->setShortcut(QKeySequence::New);
    ui->actionOpenProject->setShortcut(QKeySequence::Open);
    ui->actionOpen->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    ui->actionSaveProject->setShortcut(QKeySequence::Save);
    ui->actionSaveProjectAs->setShortcut(QKeySequence::SaveAs);
    ui->actionExport->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    ui->actionExportAs->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    ui->actionExit->setShortcut(QKeySequence::Quit);

    // Recent Projects Submenu
    m_recentProjectsMenu = new QMenu(tr("KEY_MENU_RECENT_PROJECTS"), this);
    ui->menuFile->insertMenu(ui->actionSaveProject, m_recentProjectsMenu);

    // Recent Files Submenu
    m_recentMenu = new QMenu(tr("KEY_MENU_RECENT_FILES"), this);
    ui->menuFile->insertMenu(ui->actionSaveProject, m_recentMenu);

    // Playback Space shortcut
    QShortcut *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(spaceShortcut, &QShortcut::activated, this, [this]() {
        m_animationController->togglePlayPause();
    });

    // Arrow keys stepping (deferred if slices selected)
    QShortcut *stepLeftShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(stepLeftShortcut, &QShortcut::activated, this, [this]() {
        if (m_document && !m_document->selectedFrameIndices().isEmpty()) {
            return;
        }
        m_animationController->stepBackward();
    });

    QShortcut *stepRightShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(stepRightShortcut, &QShortcut::activated, this, [this]() {
        if (m_document && !m_document->selectedFrameIndices().isEmpty()) {
            return;
        }
        m_animationController->stepForward();
    });

    // Delete shortcut
    QShortcut *deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedFrame);

    // Erase pixels and delete frames shortcut
    QShortcut *eraseShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete), this);
    connect(eraseShortcut, &QShortcut::activated, this, [this]() {
        if (m_atlasController) {
            m_atlasController->eraseSelectedSlicesPixels();
        }
    });
}

void MainWindow::setupGitHistoryDock()
{
    m_gitDock = new GitHistoryDock(this);
    m_gitDock->setProjectController(m_projectController.get());
    addDockWidget(Qt::RightDockWidgetArea, m_gitDock);

    // Create View menu between File and Help
    QMenu *viewMenu = new QMenu(tr("&Affichage"), this);
    if (ui->menuHelp) {
        ui->menuBar->insertMenu(ui->menuHelp->menuAction(), viewMenu);
    } else {
        ui->menuBar->addMenu(viewMenu);
    }

    m_actionToggleGitHistory = viewMenu->addAction(tr("Historique &Git"));
    m_actionToggleGitHistory->setCheckable(true);
    m_actionToggleGitHistory->setChecked(true);
    m_actionToggleGitHistory->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));

    connect(m_actionToggleGitHistory, &QAction::toggled, m_gitDock, &QDockWidget::setVisible);
    connect(m_gitDock, &QDockWidget::visibilityChanged, m_actionToggleGitHistory, &QAction::setChecked);
}

void MainWindow::processFile(const QString &fileName)
{
    if (m_projectController) {
        m_projectController->openFileAsync(fileName);
    }
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu || !m_projectController) return;
    m_recentMenu->clear();

    QStringList files = m_projectController->recentFiles();
    if (files.isEmpty()) {
        QAction *emptyAction = m_recentMenu->addAction(tr("KEY_ACTION_NO_RECENT_FILES"));
        emptyAction->setEnabled(false);
    } else {
        for (int i = 0; i < files.size(); ++i) {
            QString filePath = files.at(i);
            QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(filePath).fileName());
            QAction *act = m_recentMenu->addAction(text);
            act->setToolTip(filePath);
            connect(act, &QAction::triggered, this, [this, filePath]() {
                processFile(filePath);
            });
        }
        m_recentMenu->addSeparator();
        QAction *clearAction = m_recentMenu->addAction(tr("KEY_ACTION_CLEAR_RECENT_FILES"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            m_projectController->clearRecentFiles();
        });
    }
}

void MainWindow::updateRecentProjectsMenu()
{
    if (!m_recentProjectsMenu || !m_projectController) return;
    m_recentProjectsMenu->clear();

    QStringList files = m_projectController->recentProjects();
    if (files.isEmpty()) {
        QAction *emptyAction = m_recentProjectsMenu->addAction(tr("KEY_ACTION_NO_RECENT_PROJECTS"));
        emptyAction->setEnabled(false);
    } else {
        for (int i = 0; i < files.size(); ++i) {
            QString filePath = files.at(i);
            QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(filePath).fileName());
            QAction *act = m_recentProjectsMenu->addAction(text);
            act->setToolTip(filePath);
            connect(act, &QAction::triggered, this, [this, filePath]() {
                if (maybeSave()) {
                    QString err;
                    if (!m_projectController->openProject(filePath, &err)) {
                        QMessageBox::critical(this, tr("KEY_MSG_LOAD_ERROR"), err);
                    }
                }
            });
        }
        m_recentProjectsMenu->addSeparator();
        QAction *clearAction = m_recentProjectsMenu->addAction(tr("KEY_ACTION_CLEAR_RECENT_PROJECTS"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            m_projectController->clearRecentProjects();
        });
    }
}

void MainWindow::updateWindowTitle()
{
    QString name = m_projectController ? m_projectController->currentProjectName() : tr("Untitled");
    bool modified = m_projectController ? m_projectController->isProjectModified() : false;
    QString title = QStringLiteral("SpriteStudio - %1%2").arg(name, modified ? QStringLiteral(" *") : QString());
    setWindowTitle(title);
    setWindowModified(modified);
}

void MainWindow::checkCrashRecovery()
{
    QList<OrphanSessionInfo> orphans = SessionManager::detectOrphanSessions();
    if (orphans.isEmpty()) return;

    // Pick the most recent orphan session
    std::sort(orphans.begin(), orphans.end(), [](const OrphanSessionInfo &a, const OrphanSessionInfo &b) {
        return a.lastActivity > b.lastActivity;
    });

    const OrphanSessionInfo &orphan = orphans.first();
    QString timeStr = orphan.lastActivity.isValid()
        ? orphan.lastActivity.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : tr("Unknown date");
    QString msg = tr("An interrupted work session was detected:\n\nProject: %1\nDate: %2\n\nDo you want to restore this session?")
                  .arg(orphan.projectName, timeStr);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Crash Recovery"),
        msg,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QString errorMsg;
        if (!m_projectController->restoreSession(orphan.sessionDir, &errorMsg)) {
            QMessageBox::warning(this, tr("Recovery Error"), errorMsg);
        }
    } else {
        SessionManager::discardOrphanSession(orphan.sessionDir);
    }
}

bool MainWindow::maybeSave()
{
    if (!m_projectController || !m_projectController->isProjectModified()) {
        return true;
    }

    if (!m_document || m_document->isEmpty()) {
        return true;
    }

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        tr("Unsaved Changes"),
        tr("The current project '%1' has unsaved changes.\nDo you want to save them before proceeding?")
            .arg(m_projectController->currentProjectName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (ret == QMessageBox::Save) {
        on_actionSaveProject_triggered();
        return !m_projectController->isProjectModified();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

