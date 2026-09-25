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

void MainWindow::on_btnFirstFrame_clicked()
{
    if (m_animationController) {
        m_animationController->firstFrame();
    }
}

void MainWindow::on_btnPrevFrame_clicked()
{
    if (m_animationController) {
        m_animationController->stepBackward();
    }
}

void MainWindow::on_btnNextFrame_clicked()
{
    if (m_animationController) {
        m_animationController->stepForward();
    }
}

void MainWindow::on_btnLastFrame_clicked()
{
    if (m_animationController) {
        m_animationController->lastFrame();
    }
}

void MainWindow::on_btnNewAnim_clicked()
{
    if (m_animationController) {
        m_animationController->promptCreateNewAnimation();
    }
}

void MainWindow::on_btnNewFromSelection_clicked()
{
    if (m_animationController && m_document) {
        m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
    }
}

void MainWindow::on_btnDuplicateAnim_clicked()
{
    if (m_animationController) {
        m_animationController->duplicateSelectedAnimation();
    }
}

void MainWindow::on_btnReverseAnim_clicked()
{
    if (m_animationController) {
        m_animationController->reverseAnimationOrder();
    }
}

void MainWindow::on_btnDeleteAnim_clicked()
{
    if (m_animationController) {
        m_animationController->removeSelectedAnimation();
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

    QAction *duplicateAction = menu.addAction(tr("KEY_CTX_DUPLICATE_ANIM"));
    QAction *reverseAction = menu.addAction(tr("KEY_CTX_REVERSE_ANIM"));
    QAction *deleteAction = menu.addAction(tr("KEY_CTX_DELETE_ANIM"));

    bool hasSelection = (ui->animationList->currentItem() != nullptr);
    duplicateAction->setEnabled(hasSelection);
    reverseAction->setEnabled(hasSelection);
    deleteAction->setEnabled(hasSelection);

    QAction *chosen = menu.exec(ui->animationList->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == createAnimAction) {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    } else if (chosen == duplicateAction) {
        if (m_animationController) {
            m_animationController->duplicateSelectedAnimation();
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
