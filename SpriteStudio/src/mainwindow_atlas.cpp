#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include "config/appconfig.h"
#include "widgets/backgroundremovaldialog.h"
#include "widgets/atlaspackingdialog.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>

void MainWindow::on_actionToolSelect_triggered()
{
    if (m_atlasController) {
        m_atlasController->setToolMode(AtlasViewController::ToolSelect);
    }
}

void MainWindow::on_actionToolAddSlice_triggered()
{
    if (m_atlasController) {
        m_atlasController->setToolMode(AtlasViewController::ToolAddSlice);
    }
}

void MainWindow::on_actionTrimSlice_triggered()
{
    if (m_atlasController) {
        m_atlasController->trimSelectedSlice(AppConfig::instance().atlas().defaultAlphaThreshold);
    }
}

void MainWindow::on_actionZoomIn_triggered()
{
    if (zoomSlider) {
        zoomSlider->setValue(zoomSlider->value() + 25);
    }
}

void MainWindow::on_actionZoomOut_triggered()
{
    if (zoomSlider) {
        zoomSlider->setValue(zoomSlider->value() - 25);
    }
}

void MainWindow::on_actionZoomReset_triggered()
{
    if (zoomSlider) {
        zoomSlider->setValue(100);
    }
}

void MainWindow::on_actionPackAtlas_triggered()
{
    if (!m_document || m_document->atlas().isNull() || m_document->frameCount() == 0) {
        QMessageBox::information(this, tr("Empaquetage d'Atlas"),
                                 tr("Veuillez d'abord ouvrir ou importer une planche de sprites avec des frames."));
        return;
    }

    AtlasPackingDialog dlg(m_document, m_undoStack, this);
    dlg.exec();
}

void MainWindow::onBoxContextMenuRequested(int index, const QPoint &screenPos)
{
    if (!m_document || index < 0 || index >= m_document->frameCount()) return;

    QList<int> currentSel = m_document->selectedFrameIndices();
    if (!currentSel.contains(index)) {
        m_atlasController->setSelectedBoxIndices({index});
        currentSel = {index};
    }

    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("KEY_CTX_CREATE_ANIM"));
    createAnimAction->setEnabled(!currentSel.isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this, currentSel]() {
        if (m_animationController) {
            m_animationController->createAnimationFromSelection(currentSel);
        }
    });

    menu.addSeparator();

    QAction *trimAction = menu.addAction(tr("KEY_CTX_TRIM_SLICE"));
    connect(trimAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->trimSelectedSlice(AppConfig::instance().atlas().defaultAlphaThreshold);
    });

    QAction *mergeAction = menu.addAction(tr("KEY_CTX_MERGE_SLICES"));
    mergeAction->setEnabled(currentSel.size() >= 2);
    connect(mergeAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->mergeSelectedSlices();
    });

    menu.addSeparator();

    QAction *deleteAction = menu.addAction(tr("KEY_CTX_DELETE_FRAMES") + "\tDel");
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->deleteSelectedSlices();
    });

    QAction *eraseAction = menu.addAction(tr("KEY_CTX_ERASE_PIXELS") + "\tShift+Del");
    connect(eraseAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->eraseSelectedSlicesPixels();
    });

    menu.addSeparator();

    QAction *removeBgAction = menu.addAction(tr("KEY_CTX_REMOVE_BG"));
    removeBgAction->setEnabled(m_document && !m_document->atlas().isNull());
    connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    menu.addSeparator();

    QMenu *pivotMenu = menu.addMenu(tr("KEY_CTX_PIVOT_SUBMENU"));
    QAction *actGround = pivotMenu->addAction(tr("KEY_PIVOT_BOTTOMCENTER") + QStringLiteral(" (") + tr("KEY_PIVOT_GROUND_HINT") + QStringLiteral(")"));
    connect(actGround, &QAction::triggered, this, [this]() {
        applyPivotPresetToSelection(PivotPreset::BottomCenter);
    });
    QAction *actCenter = pivotMenu->addAction(tr("KEY_PIVOT_CENTER"));
    connect(actCenter, &QAction::triggered, this, [this]() {
        applyPivotPresetToSelection(PivotPreset::Center);
    });
    QAction *actTopLeft = pivotMenu->addAction(tr("KEY_PIVOT_TOPLEFT") + QStringLiteral(" (") + tr("KEY_PIVOT_UI_HINT") + QStringLiteral(")"));
    connect(actTopLeft, &QAction::triggered, this, [this]() {
        applyPivotPresetToSelection(PivotPreset::TopLeft);
    });
    pivotMenu->addSeparator();

    QMenu *morePresets = pivotMenu->addMenu(tr("KEY_CTX_MORE_PRESETS"));
    struct PresetItem { const char* key; PivotPreset preset; };
    const PresetItem items[] = {
        {"KEY_PIVOT_TOPCENTER", PivotPreset::TopCenter},
        {"KEY_PIVOT_TOPRIGHT", PivotPreset::TopRight},
        {"KEY_PIVOT_CENTERLEFT", PivotPreset::CenterLeft},
        {"KEY_PIVOT_CENTERRIGHT", PivotPreset::CenterRight},
        {"KEY_PIVOT_BOTTOMLEFT", PivotPreset::BottomLeft},
        {"KEY_PIVOT_BOTTOMRIGHT", PivotPreset::BottomRight}
    };
    for (const auto &item : items) {
        QAction *act = morePresets->addAction(tr(item.key));
        PivotPreset p = item.preset;
        connect(act, &QAction::triggered, this, [this, p]() {
            applyPivotPresetToSelection(p);
        });
    }

    menu.exec(screenPos);
}

void MainWindow::onAtlasContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("KEY_CTX_CREATE_ANIM"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this]() {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    });

    QAction *trimAction = menu.addAction(tr("KEY_CTX_TRIM_SLICE"));
    trimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(trimAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->trimSelectedSlice(AppConfig::instance().atlas().defaultAlphaThreshold);
    });

    QAction *mergeSlicesAction = menu.addAction(tr("KEY_CTX_MERGE_SLICES"));
    mergeSlicesAction->setEnabled(m_document && m_document->selectedFrameIndices().size() >= 2);
    connect(mergeSlicesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->mergeSelectedSlices();
    });

    menu.addSeparator();

    QAction *deleteFramesAction = menu.addAction(tr("KEY_CTX_DELETE_FRAMES") + "\tDel");
    deleteFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(deleteFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->deleteSelectedSlices();
    });

    QAction *eraseFramesAction = menu.addAction(tr("KEY_CTX_ERASE_PIXELS") + "\tShift+Del");
    eraseFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(eraseFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) m_atlasController->eraseSelectedSlicesPixels();
    });

    QAction *invertAction = menu.addAction(tr("KEY_CTX_INVERT_SEL"));
    invertAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(invertAction, &QAction::triggered, this, &MainWindow::invertSelection);

    menu.addSeparator();

    QAction *removeBgAction = menu.addAction(tr("KEY_CTX_REMOVE_BG"));
    removeBgAction->setEnabled(m_document && !m_document->atlas().isNull());
    connect(removeBgAction, &QAction::triggered, this, &MainWindow::removeAtlasBackgroundAndRefresh);

    QPoint globalPos = (ui->graphicsViewLayers && ui->graphicsViewLayers->viewport())
        ? ui->graphicsViewLayers->viewport()->mapToGlobal(pos)
        : pos;
    menu.exec(globalPos);
}

void MainWindow::removeAtlasBackgroundAndRefresh()
{
    if (m_document && !m_document->atlas().isNull()) {
        BackgroundRemovalDialog dlg(m_document, m_undoStack, this);
        dlg.exec();
    }
}
