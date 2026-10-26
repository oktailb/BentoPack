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

#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "include/commands/commands.h"
#include "include/extractor/extractorregistry.h"
#include "include/config/appconfig.h"
#include "image/retrolinterhelper.h"
#include "include/project/sessionmanager.h"
#include "include/widgets/githistorydock.h"
#include "include/widgets/settingsdialog.h"
#include "include/widgets/polygonmeshdialog.h"
#include "include/widgets/pixeleditordialog.h"
#include "include/widgets/branchselectiondialog.h"
#include "include/widgets/openmediaconflictdialog.h"
#include "include/widgets/watchconfigdialog.h"
#include "include/widgets/retrotileexplorerdialog.h"
#include "include/widgets/filtermenubuilder.h"
#include "include/watcher/livewatchmanager.h"
#include "include/filters/filterregistry.h"
#include <QShortcut>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QActionGroup>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QUuid>
#include <QDockWidget>
#include "generated/version.h"

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

    frameModel->setDocument(m_document);

    // Initialize configuration, extractor registry, and filter registry
    AppConfig::instance();
    ExtractorRegistry::instance();
    FilterRegistry::instance().initDefaultFilters();

    setupControllers();
    setupGitHistoryDock();
    setupErgonomicLayout();

    m_liveWatchManager = std::make_unique<LiveWatchManager>(this);
    connect(m_liveWatchManager.get(), &LiveWatchManager::stateChanged, this, &MainWindow::updateWatchStatusUI);
    connect(m_liveWatchManager.get(), &LiveWatchManager::fileReloaded, this, [this](const QString &filePath) {
        if (m_projectController) {
            m_projectController->openFile(filePath);
        }
    });
    connect(m_liveWatchManager.get(), &LiveWatchManager::exportCompleted, this, [this](bool /*success*/, const QString &/*outputPath*/, const QString &message) {
        if (statusLabel) {
            statusLabel->setText(message);
        }
    });

    setupUIConnections();
    setupShortcuts();

    // Restore saved window geometry and dock state
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
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
    const auto docks = {ui->dockPreview, ui->dockAnimations, ui->dockTimeline, ui->dockAtlasFrames, qobject_cast<QDockWidget*>(m_gitDock), m_layersDock};
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
    updatePivotUiFromSelection();

    // Enable drag-and-drop file imports across the entire window hierarchy
    enableFileDropsRecursively(this);

    // Check for crash recovery / orphan sessions
    QTimer::singleShot(100, this, &MainWindow::checkCrashRecovery);
    QTimer::singleShot(250, this, &MainWindow::checkStartupPreferences);
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
        if (m_document && !m_document->filePath().isEmpty() && m_document->filePath() == filePath) {
            if (statusLabel) statusLabel->setText(filePath);
        }
        if (frameModel) {
            frameModel->clearThumbnailCache();
        }
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
        m_animationController->updatePreview();
        if (m_timelineWidget) {
            m_timelineWidget->refresh();
        }
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers());
            if (m_layersDock && m_document && m_document->hasLayers()) {
                m_layersDock->show();
                m_layersDock->raise();
            }
        }
        refreshFrameListDisplay();
    });

    connect(m_projectController.get(), &ProjectController::projectLoaded, this, [this](const QString &/*sspPath*/) {
        updateWindowTitle();
        if (frameModel) {
            frameModel->clearThumbnailCache();
        }
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
        m_animationController->updatePreview();
        if (m_timelineWidget) {
            m_timelineWidget->refresh();
        }
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers());
        }
        refreshFrameListDisplay();
    });

    connect(m_document, &SpriteDocument::documentReset, this, [this]() {
        updateWindowTitle();
        if (frameModel) {
            frameModel->clearThumbnailCache();
        }
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
        m_animationController->updatePreview();
        if (m_timelineWidget) {
            m_timelineWidget->refresh();
        }
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers());
        }
        refreshFrameListDisplay();
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
        if (ui->statusBar) ui->statusBar->showMessage(msg, 5000);
    });

    connect(m_projectController.get(), &ProjectController::progressChanged, this, [this](int percent) {
        if (progressBar) {
            if (percent > 0) {
                progressBar->setRange(0, 100);
                progressBar->setValue(percent);
            } else {
                progressBar->setRange(0, 0);
            }
        }
    });

    connect(m_projectController.get(), &ProjectController::recentFilesChanged, this, [this]() {
        updateRecentFilesMenu();
    });

    connect(m_projectController.get(), &ProjectController::backgroundRemoved, this, [this]() {
        if (frameModel) frameModel->clearThumbnailCache();
        m_atlasController->setAtlasImage(m_document->atlas());
        populateFrameList(m_document->frames(), m_document->boxes());
        m_animationController->syncAnimationList();
        m_animationController->updatePreview();
        if (m_timelineWidget) m_timelineWidget->refresh();
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers());
        }
        refreshFrameListDisplay();
    });

    connect(m_projectController.get(), &ProjectController::processingStarted, this, [this]() {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        if (progressBar) {
            progressBar->setRange(0, 0);
            progressBar->setVisible(true);
        }
    });

    connect(m_projectController.get(), &ProjectController::processingFinished, this, [this]() {
        QApplication::restoreOverrideCursor();
        if (progressBar) {
            progressBar->setRange(0, 100);
            progressBar->setValue(100);
            QTimer::singleShot(800, this, [this]() {
                if (progressBar && (!m_projectController || !m_projectController->isProcessing())) {
                    progressBar->setValue(0);
                    progressBar->setVisible(false);
                }
            });
        }
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

        // User interactive selection on the atlas automatically creates and previews animation
        if (m_animationController) {
            m_animationController->updateCurrentAnimation(indices);
        }

        updatePivotUiFromSelection();
    });

    connect(m_document, &SpriteDocument::boxPivotChanged, this, [this](int /*index*/, const QPoint &/*pivot*/) {
        updatePivotUiFromSelection();
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
            bool wasSyncing = m_isSyncingSelection;
            m_isSyncingSelection = true;
            m_atlasController->setSelectedBoxIndices(indices);
            if (frameModel && ui->framesList && ui->framesList->selectionModel()) {
                QItemSelection selection;
                for (int row : indices) {
                    QModelIndex mIndex = frameModel->index(row, 0);
                    if (mIndex.isValid()) selection.select(mIndex, mIndex);
                }
                ui->framesList->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
                refreshFrameListDisplay();
            }
            m_isSyncingSelection = wasSyncing;
        }
    });

    connect(ui->btnPreviewFit, &QToolButton::clicked, this, [this]() {
        if (m_animationController) {
            m_animationController->fitInView();
        }
    });

    connect(ui->btnPreviewResetZoom, &QToolButton::clicked, this, [this]() {
        if (m_animationController) {
            m_animationController->resetZoom();
        }
    });

    connect(m_animationController.get(), &AnimationController::pivotDragged, this, [this](int frameIdx, const QPoint &piv) {
        if (m_document && m_document->selectedFrameIndices().contains(frameIdx)) {
            m_isSyncingPivotUi = true;
            ui->spinPivotX->setValue(piv.x());
            ui->spinPivotY->setValue(piv.y());
            m_isSyncingPivotUi = false;
        }
    });

    // Connect Document frame updates to UI model
    connect(m_document, &SpriteDocument::framesChanged, this, [this]() {
        populateFrameList(m_document->frames(), m_document->boxes());
        if (m_animationController && (m_animationController->currentAnimationName().isEmpty() || m_animationController->currentAnimationName() == QLatin1String("current"))) {
            m_animationController->updateCurrentAnimation(m_document->selectedFrameIndices());
        }
    });

    connect(m_document, &SpriteDocument::frameUpdated, this, [this](int index) {
        if (frameModel && index >= 0 && index < frameModel->rowCount()) {
            QModelIndex mIdx = frameModel->index(index, 0);
            if (mIdx.isValid()) {
                emit frameModel->dataChanged(mIdx, mIdx, {Qt::DecorationRole});
            }
        }
    });

    connect(m_document, &SpriteDocument::layersChanged, this, [this]() {
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers(), m_layerStackWidget->activeIndex());
        }
    });
}

void MainWindow::setupErgonomicLayout()
{
    // Create Timeline Filmstrip and embed into the Timeline dock
    m_timelineWidget = new TimelineFilmstripWidget(this);
    m_timelineWidget->setDocument(m_document);
    ui->timelineLayout->addWidget(m_timelineWidget);

    // Create Layers Dock and embed LayerStackWidget in document mode
    m_layersDock = new QDockWidget(tr("Layers"), this);
    m_layersDock->setObjectName(QStringLiteral("dockLayers"));
    m_layerStackWidget = new LayerStackWidget(m_layersDock);
    m_layerStackWidget->setDocumentMode(true);
    m_layersDock->setWidget(m_layerStackWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_layersDock);

    connect(m_layerStackWidget, &LayerStackWidget::layerVisibilityChanged, this, [this](int index, bool visible) {
        if (m_document) {
            m_document->setLayerVisible(index, visible);
            m_document->recompositeAllFrames();
            m_document->recompositeAtlas();
            if (m_atlasController) {
                m_atlasController->setAtlasImage(m_document->atlas());
            }
            if (frameModel) frameModel->clearThumbnailCache();
            populateFrameList(m_document->frames(), m_document->boxes());
            if (m_timelineWidget) m_timelineWidget->refresh();
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::layerLockChanged, this, [this](int index, bool locked) {
        if (m_document) {
            m_document->setLayerLocked(index, locked);
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::layerOpacityChanged, this, [this](int index, quint8 opacity) {
        if (m_document) {
            m_document->setLayerOpacity(index, opacity);
            m_document->recompositeAllFrames();
            m_document->recompositeAtlas();
            if (m_atlasController) {
                m_atlasController->setAtlasImage(m_document->atlas());
            }
            if (frameModel) frameModel->clearThumbnailCache();
            populateFrameList(m_document->frames(), m_document->boxes());
            if (m_timelineWidget) m_timelineWidget->refresh();
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::layerBlendModeChanged, this, [this](int index, QPainter::CompositionMode mode) {
        if (m_document) {
            m_document->setLayerBlendMode(index, mode);
            m_document->recompositeAllFrames();
            m_document->recompositeAtlas();
            if (m_atlasController) {
                m_atlasController->setAtlasImage(m_document->atlas());
            }
            if (frameModel) frameModel->clearThumbnailCache();
            populateFrameList(m_document->frames(), m_document->boxes());
            if (m_timelineWidget) m_timelineWidget->refresh();
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::layerNameChanged, this, [this](int index, const QString &name) {
        if (m_document) {
            m_document->setLayerName(index, name);
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::addLayerRequested, this, [this]() {
        if (m_document) {
            SpriteLayer newLyr;
            newLyr.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            newLyr.name = tr("Layer %1").arg(m_document->layerCount() + 1);
            newLyr.visible = true;
            newLyr.opacity = 255;
            newLyr.zOrder = m_document->layerCount();
            m_document->addLayer(newLyr);
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::removeLayerRequested, this, [this]() {
        if (m_document && m_layerStackWidget) {
            int active = m_layerStackWidget->activeIndex();
            if (active >= 0 && active < m_document->layerCount() && m_document->layerCount() > 1) {
                m_document->removeLayer(active);
                m_document->recompositeAllFrames();
                m_document->recompositeAtlas();
                if (m_atlasController) {
                    m_atlasController->setAtlasImage(m_document->atlas());
                }
                if (frameModel) frameModel->clearThumbnailCache();
                populateFrameList(m_document->frames(), m_document->boxes());
                if (m_timelineWidget) m_timelineWidget->refresh();
            }
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::moveLayerUpRequested, this, [this]() {
        if (m_document && m_layerStackWidget) {
            int active = m_layerStackWidget->activeIndex();
            if (active >= 0 && active < m_document->layerCount() - 1) {
                m_document->moveLayer(active, active + 1);
                m_document->recompositeAllFrames();
                m_document->recompositeAtlas();
                if (m_atlasController) {
                    m_atlasController->setAtlasImage(m_document->atlas());
                }
                if (frameModel) frameModel->clearThumbnailCache();
                populateFrameList(m_document->frames(), m_document->boxes());
                if (m_timelineWidget) m_timelineWidget->refresh();
                m_layerStackWidget->setActiveIndex(active + 1);
            }
        }
    });

    connect(m_layerStackWidget, &LayerStackWidget::moveLayerDownRequested, this, [this]() {
        if (m_document && m_layerStackWidget) {
            int active = m_layerStackWidget->activeIndex();
            if (active > 0 && active < m_document->layerCount()) {
                m_document->moveLayer(active, active - 1);
                m_document->recompositeAllFrames();
                m_document->recompositeAtlas();
                if (m_atlasController) {
                    m_atlasController->setAtlasImage(m_document->atlas());
                }
                if (frameModel) frameModel->clearThumbnailCache();
                populateFrameList(m_document->frames(), m_document->boxes());
                if (m_timelineWidget) m_timelineWidget->refresh();
                m_layerStackWidget->setActiveIndex(active - 1);
            }
        }
    });

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
    ui->actionNewProject->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    ui->actionOpenProject->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    ui->actionSaveProject->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));

    ui->actionToolSelect->setIcon(QIcon(":/drawer/icons/tool_select.png"));
    ui->actionToolAddSlice->setIcon(QIcon(":/drawer/icons/tool_slice.png"));
    ui->actionTrimSlice->setIcon(QIcon(":/drawer/icons/tool_trim.png"));
    ui->actionDetectSprites->setIcon(QIcon::fromTheme(QStringLiteral("view-grid"), QIcon(":/drawer/icons/tool_slice.png")));
    ui->actionRemoveBg->setIcon(QIcon(":/drawer/icons/tool_remove_bg.png"));

    ui->actionZoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in"), QIcon(":/drawer/plus.png")));
    ui->actionZoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out"), QIcon(":/drawer/minus.png")));

    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addAction(ui->actionToolSelect);
    toolGroup->addAction(ui->actionToolAddSlice);
    ui->actionToolSelect->setChecked(true);

    connect(ui->actionDetectSprites, &QAction::triggered, this, &MainWindow::detectSpritesAndRefresh);
    connect(ui->actionRemoveBg, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    // Live Watch toolbar toggle
    ui->mainToolBar->addSeparator();
    m_actionToggleLiveWatch = ui->mainToolBar->addAction(QIcon::fromTheme(QStringLiteral("view-refresh"), QIcon(":/drawer/icons/bentopack.png")), tr("KEY_ACTION_TOGGLE_LIVE_WATCH"));
    m_actionToggleLiveWatch->setCheckable(true);
    m_actionToggleLiveWatch->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W));
    m_actionToggleLiveWatch->setToolTip(tr("KEY_TOOLTIP_LIVE_WATCH"));
    connect(m_actionToggleLiveWatch, &QAction::triggered, this, &MainWindow::onToggleLiveWatchTriggered);

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

    m_watchStatusBtn = new QPushButton(this);
    m_watchStatusBtn->setFlat(true);
    m_watchStatusBtn->setCursor(Qt::PointingHandCursor);
    ui->statusBar->addPermanentWidget(m_watchStatusBtn);
    connect(m_watchStatusBtn, &QPushButton::clicked, this, &MainWindow::onConfigureLiveWatchTriggered);
    updateWatchStatusUI(WatchState::Idle, QString());

    // VRAM Tile Gauge widget
    m_vramGaugeWidget = new QWidget(this);
    QHBoxLayout *vramLayout = new QHBoxLayout(m_vramGaugeWidget);
    vramLayout->setContentsMargins(6, 0, 6, 0);
    vramLayout->setSpacing(6);

    m_vramLabel = new QLabel(m_vramGaugeWidget);
    m_vramLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #cbd5e1;"));
    vramLayout->addWidget(m_vramLabel);

    m_vramProgressBar = new QProgressBar(m_vramGaugeWidget);
    m_vramProgressBar->setRange(0, 100);
    m_vramProgressBar->setValue(0);
    m_vramProgressBar->setTextVisible(false);
    m_vramProgressBar->setFixedSize(70, 10);
    vramLayout->addWidget(m_vramProgressBar);

    m_vramGaugeWidget->setCursor(Qt::PointingHandCursor);
    m_vramGaugeWidget->installEventFilter(this);
    ui->statusBar->addPermanentWidget(m_vramGaugeWidget);

    connect(&AppConfig::instance(), &AppConfig::configChanged, this, &MainWindow::updateVramGauge);
    connect(m_document, &SpriteDocument::framesChanged, this, &MainWindow::updateVramGauge);
    connect(m_document, &SpriteDocument::frameUpdated, this, [this](int) { updateVramGauge(); });
    connect(m_document, &SpriteDocument::atlasChanged, this, &MainWindow::updateVramGauge);
    connect(m_document, &SpriteDocument::documentReset, this, &MainWindow::updateVramGauge);

    updateVramGauge();

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
        Q_UNUSED(deselected);
        if (m_isSyncingSelection || !m_atlasController || !m_document) return;
        m_isSyncingSelection = true;

        QList<int> indices;
        for (const QModelIndex &idx : ui->framesList->selectionModel()->selectedRows()) {
            indices.append(idx.row());
        }

        if (m_atlasController->selectedBoxIndices() != indices) {
            m_atlasController->setSelectedBoxIndices(indices);
        }
        refreshFrameListDisplay();
        m_isSyncingSelection = false;

        // Updating selection in frames list also updates the current animation preview
        if (m_animationController) {
            m_animationController->updateCurrentAnimation(indices);
        }

        // Ensure the last-clicked frame is visible on the atlas view
        if (!selected.indexes().isEmpty()) {
            m_atlasController->ensureBoxVisible(selected.indexes().first().row());
        }
        updatePivotUiFromSelection();
    });
    connect(ui->framesList, &QListView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            openPixelEditorDialog(index.row());
        }
    });
    connect(frameModel, &ArrangementModel::mergeRequested, this, &MainWindow::onMergeFrames);
    // Animation list context menu policy (slots are auto-connected by connectSlotsByName)
    ui->animationList->setContextMenuPolicy(Qt::CustomContextMenu);

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
    m_undoAction = new QAction(tr("KEY_ACTION_UNDO"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndoTriggered);
    m_editMenu->addAction(m_undoAction);

    m_redoAction = new QAction(tr("KEY_ACTION_REDO"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedoTriggered);
    m_editMenu->addAction(m_redoAction);

    if (m_undoStack) {
        connect(m_undoStack, &QUndoStack::canUndoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_undoStack, &QUndoStack::canRedoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_undoStack, &QUndoStack::indexChanged, this, &MainWindow::updateUndoRedoActions);
    }
    if (m_projectController) {
        connect(m_projectController.get(), &ProjectController::projectHistoryChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_projectController.get(), &ProjectController::fileLoaded, this, &MainWindow::updateUndoRedoActions);
        connect(m_projectController.get(), &ProjectController::projectLoaded, this, &MainWindow::updateUndoRedoActions);
    }
    updateUndoRedoActions();

    m_editMenu->addSeparator();
    m_actionPixelEditorDialog = m_editMenu->addAction(tr("KEY_ACTION_PIXEL_EDITOR"));
    m_actionPixelEditorDialog->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_actionPixelEditorDialog, &QAction::triggered, this, [this]() {
        openPixelEditorDialog();
    });

    m_actionPolygonMeshDialog = m_editMenu->addAction(tr("KEY_ACTION_POLYGON_MESH"));
    m_actionPolygonMeshDialog->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(m_actionPolygonMeshDialog, &QAction::triggered, this, [this]() {
        openPolygonMeshDialog();
    });

    m_actionMergeSlices = m_editMenu->addAction(tr("KEY_CTX_MERGE_SLICES"));
    m_actionMergeSlices->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(m_actionMergeSlices, &QAction::triggered, this, [this]() {
        if (m_atlasController) {
            m_atlasController->mergeSelectedSlices();
        }
    });

    m_editMenu->addSeparator();
    m_prefAction = m_editMenu->addAction(tr("KEY_ACTION_SETTINGS"));
    m_prefAction->setShortcut(QKeySequence::Preferences);
    connect(m_prefAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    // Create Filters Menu dynamically from FilterRegistry
    m_filtersMenu = new QMenu(tr("KEY_MENU_FILTERS"), this);
    menuBar()->insertMenu(ui->menuHelp->menuAction(), m_filtersMenu);
    FilterMenuBuilder::populateMenu(m_filtersMenu, m_document, m_undoStack, this);

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

    ui->menuFile->addSeparator();
    if (m_actionToggleLiveWatch) {
        ui->menuFile->addAction(m_actionToggleLiveWatch);
    }
    m_actionConfigureLiveWatch = ui->menuFile->addAction(tr("KEY_ACTION_CONFIGURE_LIVE_WATCH"));
    connect(m_actionConfigureLiveWatch, &QAction::triggered, this, &MainWindow::onConfigureLiveWatchTriggered);

    m_actionRetroRipper = ui->menuFile->addAction(tr("KEY_ACTION_RETRO_RIPPER"));
    connect(m_actionRetroRipper, &QAction::triggered, this, &MainWindow::onRetroRipperTriggered);

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
    if (m_layersDock) m_layersDock->setVisible(true);
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
    if (m_layersDock) {
        addDockWidget(Qt::BottomDockWidgetArea, m_layersDock);
        tabifyDockWidget(ui->dockTimeline, m_layersDock);
    }
    ui->dockTimeline->raise();

    resizeDocks({ui->dockPreview, ui->dockAnimations}, {280, 320}, Qt::Vertical);
    resizeDocks({ui->dockTimeline}, {160}, Qt::Vertical);

    saveLayoutState();
}

void MainWindow::saveLayoutState()
{
    QSettings settings(QStringLiteral("BentoPack"), QStringLiteral("BentoPack"));
    settings.setValue(QStringLiteral("mainWindow/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("mainWindow/windowState"), saveState());
}

void MainWindow::processFile(const QString &fileName)
{
    if (fileName.isEmpty()) return;

    // Check if incoming file is a retro console ROM / VRAM dump
    QFileInfo fi(fileName);
    QString ext = fi.suffix().toLower();
    static const QStringList s_retroExts = {
        QStringLiteral("smc"), QStringLiteral("sfc"),
        QStringLiteral("nes"), QStringLiteral("chr"),
        QStringLiteral("gb"),  QStringLiteral("gbc"),
        QStringLiteral("md"),  QStringLiteral("gen"), QStringLiteral("smd"),
        QStringLiteral("sms"), QStringLiteral("gg"),
        QStringLiteral("neo"), QStringLiteral("vram")
    };

    if (s_retroExts.contains(ext)) {
        RetroTileExplorerDialog dlg(fileName, this, m_projectController.get());
        dlg.exec();
        return;
    }

    if (m_document && !m_document->isEmpty()) {
        OpenMediaConflictDialog dlg(fileName, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }

        switch (dlg.selectedChoice()) {
        case OpenMediaConflictDialog::Choice::CloseAndOpenNew:
            if (maybeSave()) {
                if (m_projectController) {
                    m_projectController->openFileAsync(fileName);
                }
            }
            break;
        case OpenMediaConflictDialog::Choice::ReplaceExistingAtlas: {
            if (m_projectController) {
                m_projectController->replaceExistingAtlasWithMediaAsync(fileName);
            }
            break;
        }
        case OpenMediaConflictDialog::Choice::AddNewLayers: {
            if (m_projectController) {
                m_projectController->addMediaAsNewLayersAsync(fileName, false);
            }
            break;
        }
        case OpenMediaConflictDialog::Choice::AddAsCommonBackground: {
            if (m_projectController) {
                m_projectController->addMediaAsNewLayersAsync(fileName, true);
            }
            break;
        }
        }
        return;
    }

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
    setWindowTitle(QStringLiteral("BentoPack - %1[*]").arg(name));
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

void MainWindow::checkStartupPreferences()
{
    const AppConfig &cfg = AppConfig::instance();
    if (cfg.general().reopenLastProject && m_projectController && m_document && m_document->isEmpty()) {
        QStringList recents = m_projectController->recentProjects();
        if (!recents.isEmpty() && QFile::exists(recents.first())) {
            QString err;
            m_projectController->openProject(recents.first(), &err);
        }
    }
    if (cfg.general().checkUpdatesOnStartup) {
        checkForUpdatesSilently();
    }
}

void MainWindow::checkForUpdatesSilently()
{
    auto *nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished, this, [this, nam](QNetworkReply *reply) {
        nam->deleteLater();
        if (!reply || reply->error() != QNetworkReply::NoError) {
            if (reply) reply->deleteLater();
            return;
        }
        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;
        QJsonObject obj = doc.object();
        QString tagName = obj.value(QStringLiteral("tag_name")).toString();
        QString cleanTag = tagName;
        if (cleanTag.startsWith('v') || cleanTag.startsWith('V')) {
            cleanTag = cleanTag.mid(1);
        }
        QVersionNumber latest = QVersionNumber::fromString(cleanTag);
        QVersionNumber current = QVersionNumber::fromString(QStringLiteral(PROJECT_VERSION));
        if (latest > current && statusLabel) {
            statusLabel->setText(tr("💡 Update available: %1 (Check Settings -> Updates)").arg(tagName));
        }
    });

    QUrl url(QStringLiteral("https://api.github.com/repos/oktailb/BentoPack/releases/latest"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BentoPack/%1").arg(QStringLiteral(PROJECT_VERSION)));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    nam->get(request);
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

void MainWindow::openPolygonMeshDialog(int index)
{
    if (!m_document || m_document->frameCount() == 0) return;
    int target = index;
    if (target < 0 || target >= m_document->frameCount()) {
        const QList<int> sel = m_document->selectedFrameIndices();
        target = sel.isEmpty() ? 0 : sel.first();
    }
    BentoPackWidgets::PolygonMeshDialog dlg(m_document, m_undoStack, target, this);
    dlg.exec();
}

void MainWindow::openPixelEditorDialog(int index)
{
    if (!m_document || m_document->frameCount() == 0) return;
    int target = index;
    if (target < 0 || target >= m_document->frameCount()) {
        const QList<int> sel = m_document->selectedFrameIndices();
        target = sel.isEmpty() ? 0 : sel.first();
    }
    PixelEditorDialog dlg(m_document, m_undoStack, target, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (m_layerStackWidget) {
            m_layerStackWidget->setSpriteLayers(m_document->layers(), m_layerStackWidget->activeIndex());
        }
    }
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
    if (m_layersDock) {
        ui->menuAffichage->addAction(m_layersDock->toggleViewAction());
    }
    if (m_gitDock) {
        ui->menuAffichage->addAction(m_gitDock->toggleViewAction());
    }
    ui->menuAffichage->addSeparator();
    ui->menuAffichage->addAction(ui->mainToolBar->toggleViewAction());
    ui->menuAffichage->addSeparator();

    if (!m_actionTogglePolygonMesh) {
        m_actionTogglePolygonMesh = new QAction(tr("KEY_ACTION_TOGGLE_POLYGON_MESH"), this);
        m_actionTogglePolygonMesh->setCheckable(true);
        m_actionTogglePolygonMesh->setChecked(m_atlasController ? m_atlasController->showPolygonMeshes() : true);
        connect(m_actionTogglePolygonMesh, &QAction::toggled, this, [this](bool checked) {
            if (m_atlasController) {
                m_atlasController->setShowPolygonMeshes(checked);
            }
        });
    }
    ui->menuAffichage->addAction(m_actionTogglePolygonMesh);

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

    // Playback controls with shortcuts
    if (ui->Play) {
        ui->Play->setToolTip(tr("KEY_TOOLTIP_PLAY_PAUSE") + QStringLiteral(" (Space)"));
    }
    if (ui->Pause) {
        ui->Pause->setToolTip(tr("KEY_TOOLTIP_PAUSE") + QStringLiteral(" (Space)"));
    }
    if (ui->btnPrevFrame) {
        ui->btnPrevFrame->setToolTip(tr("KEY_TOOLTIP_PREV_FRAME") + QStringLiteral(" (Left)"));
    }
    if (ui->btnNextFrame) {
        ui->btnNextFrame->setToolTip(tr("KEY_TOOLTIP_NEXT_FRAME") + QStringLiteral(" (Right)"));
    }

    if (m_layersDock) {
        m_layersDock->setWindowTitle(tr("Layers"));
    }
    if (m_layerStackWidget) {
        m_layerStackWidget->retranslateUi();
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
    if (m_filtersMenu) {
        m_filtersMenu->setTitle(tr("KEY_MENU_FILTERS"));
        FilterMenuBuilder::populateMenu(m_filtersMenu, m_document, m_undoStack, this);
    }
    if (m_prefAction) {
        m_prefAction->setText(tr("KEY_ACTION_SETTINGS"));
    }
    if (m_helpPrefAction) {
        m_helpPrefAction->setText(tr("KEY_ACTION_SETTINGS"));
    }
    if (m_actionPolygonMeshDialog) {
        m_actionPolygonMeshDialog->setText(tr("KEY_ACTION_POLYGON_MESH"));
    }
    if (m_actionPixelEditorDialog) {
        m_actionPixelEditorDialog->setText(tr("KEY_ACTION_PIXEL_EDITOR"));
    }
    if (m_actionTogglePolygonMesh) {
        m_actionTogglePolygonMesh->setText(tr("KEY_ACTION_TOGGLE_POLYGON_MESH"));
    }
    if (m_actionRetroRipper) {
        m_actionRetroRipper->setText(tr("KEY_ACTION_RETRO_RIPPER"));
    }
    if (m_actionMergeSlices) {
        m_actionMergeSlices->setText(tr("KEY_CTX_MERGE_SLICES"));
    }
    if (m_actionToggleLiveWatch) {
        m_actionToggleLiveWatch->setText(tr("KEY_ACTION_TOGGLE_LIVE_WATCH"));
        m_actionToggleLiveWatch->setToolTip(tr("KEY_TOOLTIP_LIVE_WATCH"));
    }
    if (m_actionConfigureLiveWatch) {
        m_actionConfigureLiveWatch->setText(tr("KEY_ACTION_CONFIGURE_LIVE_WATCH"));
    }
    if (m_liveWatchManager) {
        updateWatchStatusUI(m_liveWatchManager->state(), m_liveWatchManager->lastStatusMessage());
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

    // Child docks & controllers
    if (m_gitDock) {
        m_gitDock->retranslateUi();
    }
    if (m_timelineWidget) {
        m_timelineWidget->retranslateUi();
    }
    if (m_animationController) {
        m_animationController->retranslateUi();
    }
}

void MainWindow::updatePivotUiFromSelection()
{
    if (m_isSyncingPivotUi || !m_document) return;
    m_isSyncingPivotUi = true;

    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty()) {
        ui->grpPivot->setEnabled(false);
        ui->spinPivotX->setValue(0);
        ui->spinPivotY->setValue(0);
    } else {
        ui->grpPivot->setEnabled(true);
        int firstIdx = sel.first();
        QPoint p = m_document->boxPivot(firstIdx);
        ui->spinPivotX->blockSignals(true);
        ui->spinPivotY->blockSignals(true);
        ui->spinPivotX->setValue(p.x());
        ui->spinPivotY->setValue(p.y());
        ui->spinPivotX->blockSignals(false);
        ui->spinPivotY->blockSignals(false);

        // Detect preset if applicable
        const SpriteBox &b = m_document->box(firstIdx);
        QSize sz = b.rect.size();
        int matchedPreset = 9; // Custom
        const PivotPreset presets[] = {
            PivotPreset::TopLeft, PivotPreset::TopCenter, PivotPreset::TopRight,
            PivotPreset::CenterLeft, PivotPreset::Center, PivotPreset::CenterRight,
            PivotPreset::BottomLeft, PivotPreset::BottomCenter, PivotPreset::BottomRight
        };
        for (int i = 0; i < 9; ++i) {
            if (p == SpriteBox::calculatePresetPivot(presets[i], sz)) {
                matchedPreset = i;
                break;
            }
        }
        ui->cmbPivotPreset->blockSignals(true);
        ui->cmbPivotPreset->setCurrentIndex(matchedPreset);
        ui->cmbPivotPreset->blockSignals(false);
    }

    m_isSyncingPivotUi = false;
}

void MainWindow::applyPivotPresetToSelection(PivotPreset preset)
{
    if (!m_document) return;
    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty() && m_animationController) {
        int gIdx = m_animationController->currentGlobalFrameIndex();
        if (gIdx >= 0 && gIdx < m_document->frameCount()) {
            sel = {gIdx};
        }
    }
    if (sel.isEmpty()) return;

    QList<QPoint> newPivots;
    for (int idx : sel) {
        newPivots.append(SpriteBox::calculatePresetPivot(preset, m_document->box(idx).rect.size()));
    }

    if (m_undoStack) {
        m_undoStack->push(new ChangePivotCommand(m_document, sel, newPivots, true));
    } else {
        m_document->applyPivotPreset(sel, preset);
    }
}

void MainWindow::on_btnPivotGround_clicked()
{
    applyPivotPresetToSelection(PivotPreset::BottomCenter);
}

void MainWindow::on_btnPivotCenter_clicked()
{
    applyPivotPresetToSelection(PivotPreset::Center);
}

void MainWindow::on_btnPivotTopLeft_clicked()
{
    applyPivotPresetToSelection(PivotPreset::TopLeft);
}

void MainWindow::on_btnShowReticle_toggled(bool checked)
{
    if (m_animationController) {
        m_animationController->setShowPivotReticle(checked);
    }
}

void MainWindow::on_cmbPivotPreset_currentIndexChanged(int index)
{
    if (m_isSyncingPivotUi) return;
    if (index >= 0 && index < 9) {
        const PivotPreset presets[] = {
            PivotPreset::TopLeft, PivotPreset::TopCenter, PivotPreset::TopRight,
            PivotPreset::CenterLeft, PivotPreset::Center, PivotPreset::CenterRight,
            PivotPreset::BottomLeft, PivotPreset::BottomCenter, PivotPreset::BottomRight
        };
        applyPivotPresetToSelection(presets[index]);
    }
}

void MainWindow::on_spinPivotX_valueChanged(int val)
{
    if (m_isSyncingPivotUi || !m_document) return;
    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty()) return;

    QList<QPoint> newPivots;
    for (int idx : sel) {
        QPoint p = m_document->boxPivot(idx);
        p.setX(val);
        newPivots.append(p);
    }

    if (m_undoStack) {
        m_undoStack->push(new ChangePivotCommand(m_document, sel, newPivots, true));
    } else {
        for (int i = 0; i < sel.size(); ++i) {
            m_document->setBoxPivot(sel[i], newPivots[i], true);
        }
    }
}

void MainWindow::on_spinPivotY_valueChanged(int val)
{
    if (m_isSyncingPivotUi || !m_document) return;
    QList<int> sel = m_document->selectedFrameIndices();
    if (sel.isEmpty()) return;

    QList<QPoint> newPivots;
    for (int idx : sel) {
        QPoint p = m_document->boxPivot(idx);
        p.setY(val);
        newPivots.append(p);
    }

    if (m_undoStack) {
        m_undoStack->push(new ChangePivotCommand(m_document, sel, newPivots, true));
    } else {
        for (int i = 0; i < sel.size(); ++i) {
            m_document->setBoxPivot(sel[i], newPivots[i], true);
        }
    }
}

void MainWindow::on_btnApplyPivotAnim_clicked()
{
    if (!m_document || !m_animationController) return;
    QString animName = m_animationController->currentAnimationName();
    if (animName.isEmpty() || !m_document->hasAnimation(animName)) return;

    QList<int> animFrames = m_document->animation(animName).frameIndices;
    if (animFrames.isEmpty()) return;

    QList<int> sel = m_document->selectedFrameIndices();
    QPoint targetPivot = (sel.isEmpty() ? m_document->boxPivot(animFrames.first()) : m_document->boxPivot(sel.first()));

    QList<QPoint> newPivots;
    for (int i = 0; i < animFrames.size(); ++i) {
        newPivots.append(targetPivot);
    }

    if (m_undoStack) {
        m_undoStack->push(new ChangePivotCommand(m_document, animFrames, newPivots, true));
    } else {
        m_document->setBoxesPivot(animFrames, targetPivot, true);
    }
}

void MainWindow::on_btnApplyPivotAll_clicked()
{
    if (!m_document || m_document->frameCount() == 0) return;

    QList<int> sel = m_document->selectedFrameIndices();
    QPoint targetPivot = (sel.isEmpty() ? m_document->boxPivot(0) : m_document->boxPivot(sel.first()));

    QList<int> allIndices;
    QList<QPoint> newPivots;
    for (int i = 0; i < m_document->frameCount(); ++i) {
        allIndices.append(i);
        newPivots.append(targetPivot);
    }

    if (m_undoStack) {
        m_undoStack->push(new ChangePivotCommand(m_document, allIndices, newPivots, true));
    } else {
        m_document->setBoxesPivot(allIndices, targetPivot, true);
    }
}

void MainWindow::onUndoTriggered()
{
    if (m_undoStack && m_undoStack->canUndo()) {
        m_undoStack->undo();
    } else if (m_projectController && m_projectController->canUndoGit()) {
        m_projectController->undoGit();
    }
    updateUndoRedoActions();
}

void MainWindow::onRedoTriggered()
{
    if (m_projectController && m_projectController->hasMultipleRedoBranches()) {
        QList<GitCommitInfo> branches = m_projectController->redoBranches();
        if (branches.size() == 1) {
            m_projectController->redoGit(branches.first().hash);
        } else if (!branches.isEmpty()) {
            QString selected = BranchSelectionDialog::selectBranch(branches, this);
            if (!selected.isEmpty()) {
                m_projectController->redoGit(selected);
            }
        }
        updateUndoRedoActions();
        return;
    }

    if (m_undoStack && m_undoStack->canRedo()) {
        m_undoStack->redo();
    } else if (m_projectController && m_projectController->canRedoGit()) {
        m_projectController->redoGit();
    }
    updateUndoRedoActions();
}

void MainWindow::updateUndoRedoActions()
{
    bool canUndo = (m_undoStack && m_undoStack->canUndo()) ||
                   (m_projectController && m_projectController->canUndoGit());
    if (m_undoAction) {
        m_undoAction->setEnabled(canUndo);
    }

    bool canRedo = (m_undoStack && m_undoStack->canRedo()) ||
                   (m_projectController && m_projectController->canRedoGit());
    if (m_redoAction) {
        m_redoAction->setEnabled(canRedo);
    }
}

void MainWindow::onToggleLiveWatchTriggered()
{
    if (!m_liveWatchManager) return;

    if (m_liveWatchManager->isWatching()) {
        m_liveWatchManager->stopWatching();
        if (statusLabel) statusLabel->setText(tr("Live Watch Mode stopped."));
    } else {
        LiveWatchConfig cfg = m_liveWatchManager->config();
        if (cfg.sourcePath.trimmed().isEmpty() || !QFile::exists(cfg.sourcePath)) {
            QString curPath;
            if (m_document && !m_document->filePath().isEmpty()) {
                curPath = m_document->filePath();
            } else if (m_projectController && !m_projectController->currentFilePath().isEmpty()) {
                curPath = m_projectController->currentFilePath();
            } else if (m_projectController && !m_projectController->currentProjectPath().isEmpty()) {
                curPath = m_projectController->currentProjectPath();
            }
            if (!curPath.isEmpty()) {
                cfg = LiveWatchConfig::createDefault(curPath);
            } else {
                onConfigureLiveWatchTriggered();
                return;
            }
        }
        QString err;
        if (!m_liveWatchManager->startWatching(cfg, &err)) {
            QMessageBox::warning(this, tr("Live Watch Mode"), err);
            onConfigureLiveWatchTriggered();
        } else {
            if (statusLabel) statusLabel->setText(tr("Live Watch Mode active for: %1").arg(QFileInfo(cfg.sourcePath).fileName()));
        }
    }
}

void MainWindow::onConfigureLiveWatchTriggered()
{
    if (!m_liveWatchManager) return;
    WatchConfigDialog dlg(m_liveWatchManager.get(), m_document, m_projectController.get(), this);
    dlg.exec();
}

void MainWindow::updateWatchStatusUI(WatchState state, const QString &message)
{
    if (!m_watchStatusBtn) return;

    QString tooltip = tr("Live Watch Mode — Click to configure (Ctrl+Alt+W)");
    if (!message.isEmpty()) {
        tooltip += QStringLiteral("\n") + message;
    }
    m_watchStatusBtn->setToolTip(tooltip);

    switch (state) {
    case WatchState::Idle:
        m_watchStatusBtn->setText(tr("👁 Watch: Off"));
        m_watchStatusBtn->setStyleSheet(QStringLiteral("QPushButton { color: #888888; border: none; padding: 2px 8px; border-radius: 3px; } QPushButton:hover { background: rgba(255,255,255,0.1); }"));
        if (m_actionToggleLiveWatch) {
            m_actionToggleLiveWatch->blockSignals(true);
            m_actionToggleLiveWatch->setChecked(false);
            m_actionToggleLiveWatch->blockSignals(false);
        }
        break;
    case WatchState::Watching:
        m_watchStatusBtn->setText(tr("🟢 Watch: Active"));
        m_watchStatusBtn->setStyleSheet(QStringLiteral("QPushButton { color: #4caf50; font-weight: bold; border: none; padding: 2px 8px; border-radius: 3px; } QPushButton:hover { background: rgba(76,175,80,0.2); }"));
        if (m_actionToggleLiveWatch) {
            m_actionToggleLiveWatch->blockSignals(true);
            m_actionToggleLiveWatch->setChecked(true);
            m_actionToggleLiveWatch->blockSignals(false);
        }
        break;
    case WatchState::Compiling:
        m_watchStatusBtn->setText(tr("⚙ Watch: Compiling..."));
        m_watchStatusBtn->setStyleSheet(QStringLiteral("QPushButton { color: #ff9800; font-weight: bold; border: none; padding: 2px 8px; border-radius: 3px; } QPushButton:hover { background: rgba(255,152,0,0.2); }"));
        break;
    case WatchState::Success: {
        QString timeStr = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
        m_watchStatusBtn->setText(tr("✅ Watch: Ready (%1)").arg(timeStr));
        m_watchStatusBtn->setStyleSheet(QStringLiteral("QPushButton { color: #4caf50; border: none; padding: 2px 8px; border-radius: 3px; } QPushButton:hover { background: rgba(76,175,80,0.2); }"));
        if (m_actionToggleLiveWatch) {
            m_actionToggleLiveWatch->blockSignals(true);
            m_actionToggleLiveWatch->setChecked(true);
            m_actionToggleLiveWatch->blockSignals(false);
        }
        break;
    }
    case WatchState::Error:
        m_watchStatusBtn->setText(tr("⚠️ Watch: Error"));
        m_watchStatusBtn->setStyleSheet(QStringLiteral("QPushButton { color: #f44336; font-weight: bold; border: none; padding: 2px 8px; border-radius: 3px; } QPushButton:hover { background: rgba(244,67,54,0.2); }"));
        break;
    }
}

void MainWindow::onRetroRipperTriggered()
{
    QString filter = tr("Retro ROMs & VRAM (*.smc *.sfc *.nes *.chr *.gb *.gbc *.md *.gen *.smd *.sms *.gg *.neo *.vram *.bin);;All Files (*.*)");
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Retro ROM or VRAM Dump"), "", filter);
    if (!fileName.isEmpty()) {
        RetroTileExplorerDialog dlg(fileName, this, m_projectController.get());
        dlg.exec();
    }
void MainWindow::updateVramGauge()
{
    if (!m_vramGaugeWidget || !m_vramLabel || !m_vramProgressBar) return;

    const auto &retro = AppConfig::instance().retroLinter();
    if (!retro.enabled || !retro.showVramGauge) {
        m_vramGaugeWidget->setVisible(false);
        return;
    }

    m_vramGaugeWidget->setVisible(true);

    int uniqueTiles = 0;
    if (m_document && m_document->frameCount() > 0) {
        uniqueTiles = RetroLinterHelper::countUniqueTiles(m_document->frames(), retro.tileSize);
    }

    int budget = retro.maxVramTiles > 0 ? retro.maxVramTiles : 256;
    int percent = (budget > 0) ? ((uniqueTiles * 100) / budget) : 0;

    m_vramLabel->setText(QStringLiteral("VRAM: %1 / %2 tiles").arg(uniqueTiles).arg(budget));
    m_vramProgressBar->setValue(std::min(100, percent));

    if (percent > 100) {
        m_vramProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #334155; border: 1px solid #475569; border-radius: 3px; }"
            "QProgressBar::chunk { background: #ef4444; border-radius: 2px; }"));
        m_vramLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #f87171;"));
    } else if (percent >= 85) {
        m_vramProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #334155; border: 1px solid #475569; border-radius: 3px; }"
            "QProgressBar::chunk { background: #f59e0b; border-radius: 2px; }"));
        m_vramLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #fbbf24;"));
    } else {
        m_vramProgressBar->setStyleSheet(QStringLiteral(
            "QProgressBar { background: #334155; border: 1px solid #475569; border-radius: 3px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #06b6d4, stop:1 #10b981); border-radius: 2px; }"));
        m_vramLabel->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: bold; color: #cbd5e1;"));
    }

    QString profileName;
    switch (retro.profileIndex) {
    case 1: profileName = QStringLiteral("Nintendo Game Boy (DMG/CGB)"); break;
    case 2: profileName = QStringLiteral("Nintendo NES / Famicom"); break;
    case 3: profileName = QStringLiteral("Sega Master System"); break;
    case 4: profileName = QStringLiteral("Sega Mega Drive / Genesis"); break;
    case 5: profileName = QStringLiteral("Commodore 64"); break;
    case 6: profileName = QStringLiteral("Super Nintendo (SNES)"); break;
    case 7: profileName = QStringLiteral("PICO-8"); break;
    default: profileName = QStringLiteral("Custom / Generic Retro"); break;
    }

    QString tip = QStringLiteral(
        "<b>VRAM Tile Budget:</b> %1 / %2 unique %3x%3 tiles (%4%)\n"
        "<b>Scanline Sprite Limit:</b> %5 px\n"
        "<b>Hardware Profile:</b> %6\n"
        "<i>Click to configure in Settings</i>")
        .arg(uniqueTiles)
        .arg(budget)
        .arg(retro.tileSize)
        .arg(percent)
        .arg(retro.scanlineSpriteWidthLimit)
        .arg(profileName);

    m_vramGaugeWidget->setToolTip(tip);
}




