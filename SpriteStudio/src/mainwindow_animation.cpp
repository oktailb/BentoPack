#include "include/mainwindow.h"
#include "ui_mainwindow.h"
#include <QMenu>
#include <QAction>

void MainWindow::on_Play_clicked()
{
    if (m_animationController) {
        m_animationController->play();
    }
}

void MainWindow::on_Pause_clicked()
{
    if (m_animationController) {
        m_animationController->pause();
    }
}

void MainWindow::on_fps_valueChanged(int fps)
{
    if (m_animationController) {
        m_animationController->setFps(fps);
    }
}

void MainWindow::on_animationList_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("KEY_CTX_CREATE_ANIM"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());

    QAction *reverseAction = menu.addAction(tr("KEY_CTX_REVERSE_ANIM"));
    QAction *deleteAction = menu.addAction(tr("KEY_CTX_DELETE_ANIM"));

    QAction *chosen = menu.exec(ui->animationList->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == createAnimAction) {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    } else if (chosen == reverseAction) {
        if (m_animationController) {
            m_animationController->reverseAnimationOrder();
        }
    } else if (chosen == deleteAction) {
        if (m_animationController) {
            m_animationController->removeSelectedAnimation();
        }
    }
}
