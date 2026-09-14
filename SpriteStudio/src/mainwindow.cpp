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
#include <QActionGroup>

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
    setupGitHistoryDock();
    setupErgonomicLayout();
    setupUIConnections();
    setupShortcuts();

    // Restore saved window geometry and dock state
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    QByteArray savedGeometry = settings.value(QStringLiteral("mainWindow/geometry")).toByteArray();
    QByteArray savedState = settings.value(QStringLiteral("mainWindow/windowState")).toByteArray();
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }
    bool stateRestored = false;
    if (!savedState.isEmpty()) {
        stateRestored = restoreState(savedState);
    }
    if (!stateRestored) {
        resetDefaultLayout();
    }

    // Auto-save dock layout whenever panels are moved, closed, docked or floated
    const auto docks = {ui->dockPreview, ui->dockAnimations, ui->dockTimeline, ui->dockAtlasFrames, qobject_cast<QDockWidget*>(m_gitDock)};
    for (QDockWidget *dock : docks) {
        if (!dock) continue;
        connect(dock, &QDockWidget::visibilityChanged, this, [this](bool) {
            saveLayoutState();
        });
        connect(dock, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea) {
            saveLayoutState();
        });
        connect(dock, &QDockWidget::topLevelChanged, this, [this](bool) {
            saveLayoutState();
        });
    }

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
        ui->actionToolSelect->setChecked(mode == AtlasViewController::ToolSelect);
        ui->actionToolAddSlice->setChecked(mode == AtlasViewController::ToolAddSlice);
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
        ui->timingLabel->setText(" -> " + tr("KEY_LABEL_TIMING") + ": " +
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

void MainWindow::setupErgonomicLayout()
{
    // Create Timeline Filmstrip and embed into the Timeline dock
    m_timelineWidget = new TimelineFilmstripWidget(this);
    m_timelineWidget->setDocument(m_document);
    ui->timelineLayout->addWidget(m_timelineWidget);

    // Attach timeline, scrubber, and loop mode to AnimationController
    if (m_animationController) {
        m_animationController->attachTimelineWidget(m_timelineWidget);
        m_animationController->attachScrubberSlider(ui->sliderScrubber, ui->lblFrameCounter);
        m_animationController->attachLoopModeComboBox(ui->comboLoopMode);
    }

    // Configure animationList columns
    ui->animationList->setHeaderLabels({tr("KEY_ANIM_COL_NAME"), tr("KEY_ANIM_COL_FPS"), tr("KEY_ANIM_COL_MODE"), tr("KEY_ANIM_COL_FRAMES"), tr("KEY_ANIM_COL_DURATION")});
    ui->animationList->header()->resizeSection(0, 110);
    ui->animationList->header()->resizeSection(1, 45);
    ui->animationList->header()->resizeSection(2, 65);
    ui->animationList->header()->resizeSection(3, 50);
    ui->animationList->header()->resizeSection(4, 55);

    // Setup main toolbar
    ui->mainToolBar->setWindowTitle(tr("KEY_TOOLBAR_MAIN"));
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->actionNewProject->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentNew));
    ui->actionOpenProject->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));
    ui->actionSaveProject->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave));

    ui->actionToolSelect->setIcon(QIcon(":/drawer/icons/tool_select.png"));
    ui->actionToolAddSlice->setIcon(QIcon(":/drawer/icons/tool_slice.png"));
    ui->actionTrimSlice->setIcon(QIcon(":/drawer/icons/tool_trim.png"));
    ui->actionRemoveBg->setIcon(QIcon(":/drawer/icons/tool_remove_bg.png"));

    ui->actionZoomIn->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ZoomIn, QIcon(":/drawer/plus.png")));
    ui->actionZoomOut->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ZoomOut, QIcon(":/drawer/minus.png")));

    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addAction(ui->actionToolSelect);
    toolGroup->addAction(ui->actionToolAddSlice);
    ui->actionToolSelect->setChecked(true);

    connect(ui->actionToolSelect, &QAction::triggered, this, &MainWindow::on_actionToolSelect_triggered);
    connect(ui->actionToolAddSlice, &QAction::triggered, this, &MainWindow::on_actionToolAddSlice_triggered);
    connect(ui->actionTrimSlice, &QAction::triggered, this, &MainWindow::on_actionTrimSlice_triggered);
    connect(ui->actionRemoveBg, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    connect(ui->actionZoomIn, &QAction::triggered, this, &MainWindow::on_actionZoomIn_triggered);
    connect(ui->actionZoomOut, &QAction::triggered, this, &MainWindow::on_actionZoomOut_triggered);
    connect(ui->actionZoomReset, &QAction::triggered, this, &MainWindow::on_actionZoomReset_triggered);

    // Setup Affichage (View) Menu with toggle actions for all docks
    setupViewMenuActions();
    connect(ui->actionResetLayout, &QAction::triggered, this, &MainWindow::resetDefaultLayout);

    // Connect animation selection to automatically show and raise the Timeline dock
    connect(m_animationController.get(), &AnimationController::currentAnimationChanged, this, [this](const QString &name) {
        if (!name.isEmpty() && ui->dockTimeline) {
            ui->dockTimeline->setVisible(true);
            ui->dockTimeline->raise();
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
            this, [this](const QItemSelection &selected, const QItemSelection &deselected) {
        if (m_isSyncingSelection || !m_atlasController || !m_document) return;
        m_isSyncingSelection = true;

        QList<int> indices = m_atlasController->selectedBoxIndices();

        for (const QModelIndex &idx : deselected.indexes()) {
            indices.removeAll(idx.row());
        }
        for (const QModelIndex &idx : selected.indexes()) {
            int r = idx.row();
            if (!indices.contains(r)) {
                indices.append(r);
            }
        }

        if (indices.isEmpty()) {
            for (const QModelIndex &idx : ui->framesList->selectionModel()->selectedRows()) {
                indices.append(idx.row());
            }
        }

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

    // Animation list context menu
    ui->animationList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->animationList, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::on_animationList_customContextMenuRequested);

    // Animation action buttons
    connect(ui->btnNewAnim, &QToolButton::clicked, this, &MainWindow::on_btnNewAnim_clicked);
    connect(ui->btnNewFromSelection, &QToolButton::clicked, this, &MainWindow::on_btnNewFromSelection_clicked);
    connect(ui->btnDuplicateAnim, &QToolButton::clicked, this, &MainWindow::on_btnDuplicateAnim_clicked);
    connect(ui->btnReverseAnim, &QToolButton::clicked, this, &MainWindow::on_btnReverseAnim_clicked);
    connect(ui->btnDeleteAnim, &QToolButton::clicked, this, &MainWindow::on_btnDeleteAnim_clicked);

    // Transport buttons
    connect(ui->btnFirstFrame, &QToolButton::clicked, this, &MainWindow::on_btnFirstFrame_clicked);
    connect(ui->btnPrevFrame, &QToolButton::clicked, this, &MainWindow::on_btnPrevFrame_clicked);
    connect(ui->btnNextFrame, &QToolButton::clicked, this, &MainWindow::on_btnNextFrame_clicked);
    connect(ui->btnLastFrame, &QToolButton::clicked, this, &MainWindow::on_btnLastFrame_clicked);
    connect(ui->Play, &QPushButton::clicked, this, &MainWindow::on_Play_clicked);
    connect(ui->Pause, &QPushButton::clicked, this, &MainWindow::on_Pause_clicked);
    connect(ui->fps, &QSpinBox::valueChanged, this, &MainWindow::on_fps_valueChanged);

    // Enforce fixed widths to guarantee absolute position stability when timing changes
    ui->fpsLabel->setFixedWidth(28);
    ui->fps->setFixedWidth(55);
    ui->timingLabel->setFixedWidth(120);
    ui->timingLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->timingLabel->setText(" -> " + tr("KEY_LABEL_TIMING") + ": " +
                             QString::number(1000.0 / static_cast<double>(ui->fps->value()), 'g', 4) + "ms");
}

void MainWindow::setupShortcuts()
{
    // Create Edit Menu for Undo/Redo
    m_editMenu = new QMenu(tr("KEY_MENU_EDIT"), this);
    menuBar()->insertMenu(ui->menuHelp->menuAction(), m_editMenu);
    m_undoAction = m_undoStack->createUndoAction(this, tr("KEY_ACTION_UNDO"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_editMenu->addAction(m_undoAction);

    m_redoAction = m_undoStack->createRedoAction(this, tr("KEY_ACTION_REDO"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_editMenu->addAction(m_redoAction);

    m_editMenu->addSeparator();
    m_removeBgAction = m_editMenu->addAction(tr("KEY_ACTION_REMOVE_BG"));
    m_removeBgAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(m_removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    m_editMenu->addSeparator();
    m_prefAction = m_editMenu->addAction(tr("KEY_ACTION_SETTINGS"));
    m_prefAction->setShortcut(QKeySequence::Preferences);
    connect(m_prefAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    ui->menuHelp->addSeparator();
    m_helpPrefAction = ui->menuHelp->addAction(tr("KEY_ACTION_SETTINGS"));
    connect(m_helpPrefAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

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

    // Arrow keys shortcuts for next / prev frame
    QShortcut *prevFrameShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(prevFrameShortcut, &QShortcut::activated, this, [this]() {
        if (m_animationController) {
            m_animationController->stepBackward();
        }
    });

    QShortcut *nextFrameShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(nextFrameShortcut, &QShortcut::activated, this, [this]() {
        if (!m_animationController) return;
        if (!m_document || m_document->selectedFrameIndices().size() <= 1) {
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
    m_gitDock->setObjectName(QStringLiteral("gitHistoryDock"));
    m_gitDock->setWindowTitle(tr("KEY_DOCK_GIT_HISTORY"));
    m_gitDock->setProjectController(m_projectController.get());
    addDockWidget(Qt::RightDockWidgetArea, m_gitDock);

    m_actionToggleGitHistory = m_gitDock->toggleViewAction();
    m_actionToggleGitHistory->setText(tr("KEY_DOCK_GIT_HISTORY"));
    m_actionToggleGitHistory->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
}

void MainWindow::resetDefaultLayout()
{
    if (ui->dockPreview) ui->dockPreview->setVisible(true);
    if (ui->dockAnimations) ui->dockAnimations->setVisible(true);
    if (ui->dockTimeline) ui->dockTimeline->setVisible(true);
    if (ui->dockAtlasFrames) ui->dockAtlasFrames->setVisible(true);
    if (m_gitDock) m_gitDock->setVisible(true);
    if (ui->mainToolBar) ui->mainToolBar->setVisible(true);

    addDockWidget(Qt::RightDockWidgetArea, ui->dockPreview);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockAnimations);
    splitDockWidget(ui->dockPreview, ui->dockAnimations, Qt::Vertical);

    if (m_gitDock) {
        addDockWidget(Qt::RightDockWidgetArea, m_gitDock);
        tabifyDockWidget(ui->dockAnimations, m_gitDock);
        ui->dockAnimations->raise();
    }

    addDockWidget(Qt::BottomDockWidgetArea, ui->dockTimeline);
    addDockWidget(Qt::BottomDockWidgetArea, ui->dockAtlasFrames);
    tabifyDockWidget(ui->dockTimeline, ui->dockAtlasFrames);
    ui->dockTimeline->raise();

    resizeDocks({ui->dockPreview, ui->dockAnimations}, {280, 320}, Qt::Vertical);
    resizeDocks({ui->dockTimeline}, {160}, Qt::Vertical);

    saveLayoutState();
}

void MainWindow::saveLayoutState()
{
    QSettings settings(QStringLiteral("SpriteStudio"), QStringLiteral("SpriteStudio"));
    settings.setValue(QStringLiteral("mainWindow/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("mainWindow/windowState"), saveState());
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
    QString name = m_projectController ? m_projectController->currentProjectName() : tr("KEY_UNTITLED_PROJECT");
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
        : tr("KEY_UNKNOWN_DATE");
    QString msg = tr("KEY_RECOVERY_PROMPT")
                  .arg(orphan.projectName, timeStr);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("KEY_RECOVERY_TITLE"),
        msg,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QString errorMsg;
        if (!m_projectController->restoreSession(orphan.sessionDir, &errorMsg)) {
            QMessageBox::warning(this, tr("KEY_RECOVERY_ERROR"), errorMsg);
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
        tr("KEY_UNSAVED_CHANGES_TITLE"),
        tr("KEY_UNSAVED_CHANGES_PROMPT")
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

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setupViewMenuActions()
{
    if (!ui || !ui->menuAffichage) return;
    ui->menuAffichage->clear();
    ui->menuAffichage->addAction(ui->dockPreview->toggleViewAction());
    ui->menuAffichage->addAction(ui->dockAnimations->toggleViewAction());
    ui->menuAffichage->addAction(ui->dockTimeline->toggleViewAction());
    ui->menuAffichage->addAction(ui->dockAtlasFrames->toggleViewAction());
    if (m_gitDock) {
        ui->menuAffichage->addAction(m_gitDock->toggleViewAction());
    }
    ui->menuAffichage->addSeparator();
    ui->menuAffichage->addAction(ui->mainToolBar->toggleViewAction());
    ui->menuAffichage->addSeparator();
    ui->menuAffichage->addAction(ui->actionResetLayout);
}

void MainWindow::retranslateUi()
{
    ui->retranslateUi(this);

    // Main Toolbar title
    if (ui->mainToolBar) {
        ui->mainToolBar->setWindowTitle(tr("KEY_TOOLBAR_MAIN"));
    }

    // Animation list headers
    if (ui->animationList) {
        ui->animationList->setHeaderLabels({
            tr("KEY_ANIM_COL_NAME"),
            tr("KEY_ANIM_COL_FRAMES"),
            tr("KEY_ANIM_COL_FPS"),
            tr("KEY_ANIM_COL_DURATION")
        });
    }

    // Dynamic menus & actions
    if (m_recentMenu) {
        m_recentMenu->setTitle(tr("KEY_MENU_RECENT_FILES"));
    }
    if (m_recentProjectsMenu) {
        m_recentProjectsMenu->setTitle(tr("KEY_MENU_RECENT_PROJECTS"));
    }
    if (m_editMenu) {
        m_editMenu->setTitle(tr("KEY_MENU_EDIT"));
    }
    if (m_undoAction) {
        m_undoAction->setText(tr("KEY_ACTION_UNDO"));
    }
    if (m_redoAction) {
        m_redoAction->setText(tr("KEY_ACTION_REDO"));
    }
    if (m_removeBgAction) {
        m_removeBgAction->setText(tr("KEY_ACTION_REMOVE_BG"));
    }
    if (m_prefAction) {
        m_prefAction->setText(tr("KEY_ACTION_SETTINGS"));
    }
    if (m_helpPrefAction) {
        m_helpPrefAction->setText(tr("KEY_ACTION_SETTINGS"));
    }

    // Refresh view menu dock titles
    setupViewMenuActions();

    // Status bar widgets
    if (progressBar) {
        progressBar->setFormat(tr("KEY_STATUS_PROGRESS") + QStringLiteral(" %p%"));
    }
    if (ui->timingLabel) {
        ui->timingLabel->setText(QStringLiteral(" -> ") + tr("KEY_LABEL_TIMING") + QStringLiteral(": ") +
                                 QString::number(1000.0 / static_cast<double>(ui->fps->value()), 'g', 4) + QStringLiteral("ms"));
    }

    // Recent menus actions
    updateRecentProjectsMenu();
    updateRecentFilesMenu();

    // Frame list items
    refreshFrameListDisplay();

    // Window title
    updateWindowTitle();

    // Child docks
    if (m_gitDock) {
        m_gitDock->retranslateUi();
    }
    if (m_timelineWidget) {
        m_timelineWidget->retranslateUi();
    }
}


