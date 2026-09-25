/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QMenu>
#include <QUndoStack>
#include <QStyledItemDelegate>
#include <QPainter>
#include <memory>

#include "arrangementmodel.h"
#include "model/spritedocument.h"
#include "animation/animationplayer.h"
#include "controller/projectcontroller.h"
#include "controller/animationcontroller.h"
#include "controller/atlasviewcontroller.h"
#include "widgets/timelinefilmstripwidget.h"
#include "bentopackwidgets_export.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class GitHistoryDock;

/**
 * @brief Custom item delegate for drawing visual feedback in the frame list view.
 */
class FrameDelegate : public QStyledItemDelegate
{
public:
    enum HighlightState {
        None,
        Merge,
        InsertLeft,
        InsertRight
    };

    explicit FrameDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void setHighlight(int row, HighlightState state) {
        m_targetRow = row;
        m_state = state;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::paint(painter, option, index);

        if (index.row() != m_targetRow || m_state == None) {
            return;
        }

        painter->save();
        if (m_state == Merge) {
            QPen pen(Qt::magenta);
            pen.setWidth(3);
            pen.setStyle(Qt::DotLine);
            QRect rect = option.rect.adjusted(2, 2, -2, -2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect);
        } else if (m_state == InsertLeft || m_state == InsertRight) {
            int xPos = (m_state == InsertLeft) ? option.rect.left() : option.rect.right();
            if (m_state == InsertRight) xPos -= 2;
            painter->setPen(Qt::NoPen);
            painter->setBrush(Qt::black);
            painter->drawRect(xPos, option.rect.top(), 3, option.rect.height());
        }
        painter->restore();
    }

private:
    int             m_targetRow = -1;
    HighlightState  m_state = None;
};

/**
 * @brief The main window orchestrator for the BentoPack application.
 *
 * Coordinates UI views with specialized controllers:
 * - ProjectController: File I/O, codecs, recent files, and background removal.
 * - AtlasViewController: Atlas QGraphicsView, zoom/pan, slicing tools, and interactive boxes.
 * - AnimationController: Animation playback, tree widget, preview rendering, and animation CRUD.
 */
class BENTOPACK_WIDGETS_EXPORT MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    using SliceToolMode = AtlasViewController::SliceToolMode;

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void processFile(const QString &fileName);

    ProjectController* projectController() const { return m_projectController.get(); }
    AtlasViewController* atlasController() const { return m_atlasController.get(); }
    AnimationController* animationController() const { return m_animationController.get(); }

protected:
    void closeEvent(QCloseEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    void changeEvent(QEvent *event) override;
    void resetDefaultLayout();

private slots:
    // Menu actions
    void on_actionNewProject_triggered();
    void on_actionOpenProject_triggered();
    void on_actionSaveProject_triggered();
    void on_actionSaveProjectAs_triggered();
    void on_actionExportAs_triggered();
    void on_actionLicence_triggered();
    void on_actionAbout_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionExport_triggered();
    void on_actionExit_triggered();
    void openSettingsDialog();
    void openPolygonMeshDialog(int index = -1);
    void openPixelEditorDialog(int index = -1);

    // Playback & FPS
    void on_Play_clicked();
    void on_Pause_clicked();
    void on_btnFirstFrame_clicked();
    void on_btnPrevFrame_clicked();
    void on_btnNextFrame_clicked();
    void on_btnLastFrame_clicked();
    void on_btnNewAnim_clicked();
    void on_btnNewFromSelection_clicked();
    void on_btnDuplicateAnim_clicked();
    void on_btnReverseAnim_clicked();
    void on_btnDeleteAnim_clicked();
    void on_fps_valueChanged(int fps);
    void zoomSliderChanged(int val);

    // Frame list
    void onMergeFrames(int sourceRow, int targetRow);
    void on_framesList_customContextMenuRequested(const QPoint &pos);
    void deleteSelectedFrame();
    void invertSelection();

    // Slicing tools
    void on_actionToolSelect_triggered();
    void on_actionToolAddSlice_triggered();
    void on_actionTrimSlice_triggered();
    void on_actionZoomIn_triggered();
    void on_actionZoomOut_triggered();
    void on_actionZoomReset_triggered();
    void on_actionPackAtlas_triggered();

    // Context menus
    void onAtlasContextMenuRequested(const QPoint &pos);
    void onBoxContextMenuRequested(int index, const QPoint &screenPos);
    void on_animationList_customContextMenuRequested(const QPoint &pos);

    // Image processing
    void removeAtlasBackgroundAndRefresh();

    // Pivot controls
    void on_btnPivotGround_clicked();
    void on_btnPivotCenter_clicked();
    void on_btnPivotTopLeft_clicked();
    void on_btnShowReticle_toggled(bool checked);
    void on_cmbPivotPreset_currentIndexChanged(int index);
    void on_spinPivotX_valueChanged(int val);
    void on_spinPivotY_valueChanged(int val);
    void on_btnApplyPivotAnim_clicked();
    void on_btnApplyPivotAll_clicked();

    void onUndoTriggered();
    void onRedoTriggered();
    void updateUndoRedoActions();

private:
    void updatePivotUiFromSelection();
    void applyPivotPresetToSelection(PivotPreset preset);
    void setupControllers();
    void setupErgonomicLayout();
    void setupUIConnections();
    void setupShortcuts();
    void setupGitHistoryDock();
    void updateRecentFilesMenu();
    void updateRecentProjectsMenu();
    void updateWindowTitle();
    void checkCrashRecovery();
    void checkStartupPreferences();
    void checkForUpdatesSilently();
    bool maybeSave();
    void saveLayoutState();
    void syncFromDocument();
    void populateFrameList(const QList<QImage> &frameList, const QList<SpriteBox> &boxList);
    void refreshFrameListDisplay();
    void setupViewMenuActions();
    void retranslateUi();

    std::unique_ptr<Ui::MainWindow> ui;
    ArrangementModel *frameModel = nullptr;
    FrameDelegate *listDelegate = nullptr;

    SpriteDocument *m_document = nullptr;
    QUndoStack *m_undoStack = nullptr;
    AnimationPlayer *m_player = nullptr;

    std::unique_ptr<ProjectController> m_projectController;
    std::unique_ptr<AtlasViewController> m_atlasController;
    std::unique_ptr<AnimationController> m_animationController;

    QMenu *m_recentMenu = nullptr;
    QMenu *m_recentProjectsMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_filtersMenu = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_prefAction = nullptr;
    QAction *m_helpPrefAction = nullptr;
    GitHistoryDock *m_gitDock = nullptr;
    QAction *m_actionToggleGitHistory = nullptr;
    QAction *m_actionTogglePolygonMesh = nullptr;
    QAction *m_actionPolygonMeshDialog = nullptr;
    QAction *m_actionPixelEditorDialog = nullptr;
    QAction *m_actionMergeSlices = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *zoomLabel = nullptr;
    QSlider *zoomSlider = nullptr;
    QProgressBar *progressBar = nullptr;
    TimelineFilmstripWidget *m_timelineWidget = nullptr;
    bool m_isSyncingSelection = false;
    bool m_isSyncingPivotUi = false;
};

#endif // MAINWINDOW_H
