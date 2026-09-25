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
#include "include/aboutdialog.h"
#include "include/extractor/extractorregistry.h"
#include "include/commands/commands.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

static QString readTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("Error: Cannot open file %1").arg(filePath);
    }
    QTextStream in(&file);
    return in.readAll();
}

void MainWindow::on_actionLicence_triggered()
{
    QString licenseText = readTextFile(":/text/license.txt");
    QDialog dialog;
    dialog.setWindowTitle(tr("KEY_DIALOG_LICENCE_TITLE"));
    dialog.setMinimumSize(600, 400);
    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setPlainText(licenseText);
    textEdit->setReadOnly(true);
    QPushButton *closeButton = new QPushButton(tr("KEY_DIALOG_ABOUT_CLOSE"), &dialog);
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(textEdit);
    layout->addWidget(closeButton);
    dialog.exec();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::on_actionNewProject_triggered()
{
    if (!maybeSave()) return;
    if (m_projectController) {
        m_projectController->newProject();
    }
}

void MainWindow::on_actionOpenProject_triggered()
{
    if (!maybeSave()) return;

    QString initialDir = m_projectController && !m_projectController->currentProjectPath().isEmpty()
        ? QFileInfo(m_projectController->currentProjectPath()).absolutePath()
        : QDir::homePath();

    QString filter = tr("BentoPack (*.bento);;All Files (*.*)");
    QString fileName = QFileDialog::getOpenFileName(this, tr("KEY_ACTION_OPEN_PROJECT"), initialDir, filter);
    if (!fileName.isEmpty() && m_projectController) {
        QString errorMsg;
        if (!m_projectController->openProject(fileName, &errorMsg)) {
            QMessageBox::critical(this, tr("KEY_MSG_LOAD_ERROR"), errorMsg);
        }
    }
}

void MainWindow::on_actionSaveProject_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("KEY_ACTION_SAVE_PROJECT"), tr("KEY_MSG_NOTHING_TO_SAVE"));
        return;
    }

    if (m_projectController->currentProjectPath().isEmpty()) {
        on_actionSaveProjectAs_triggered();
        return;
    }

    QString errorMsg;
    if (!m_projectController->saveProject(m_projectController->currentProjectPath(), &errorMsg)) {
        QMessageBox::critical(this, tr("KEY_MSG_SAVE_ERROR"), errorMsg);
    }
}

void MainWindow::on_actionSaveProjectAs_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("KEY_ACTION_SAVE_PROJECT_AS"), tr("KEY_MSG_NOTHING_TO_SAVE"));
        return;
    }

    QString initialPath = m_projectController->currentProjectPath().isEmpty()
        ? QDir::homePath() + QStringLiteral("/project.bento")
        : m_projectController->currentProjectPath();

    QString filter = tr("BentoPack (*.bento);;All Files (*.*)");
    QString fileName = QFileDialog::getSaveFileName(this, tr("KEY_ACTION_SAVE_PROJECT_AS"), initialPath, filter);
    if (!fileName.isEmpty()) {
        QString errorMsg;
        if (!m_projectController->saveProjectAs(fileName, &errorMsg)) {
            QMessageBox::critical(this, tr("KEY_MSG_SAVE_ERROR"), errorMsg);
        }
    }
}

void MainWindow::on_actionExportAs_triggered()
{
    on_actionExport_triggered();
}

void MainWindow::on_actionOpen_triggered()
{
    if (!maybeSave()) return;

    const QString title = tr("KEY_DIALOG_OPEN_TITLE");
    const QString formats = ExtractorRegistry::instance().openFilterString();
    QString fileName = QFileDialog::getOpenFileName(this, title, "", formats);
    if (!fileName.isEmpty()) {
        processFile(fileName);
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("KEY_ACTION_SAVE"), tr("KEY_MSG_NOTHING_TO_SAVE"));
        return;
    }

    QString currentPath = m_projectController->currentFilePath();
    if (currentPath.isEmpty()) {
        on_actionExport_triggered();
        return;
    }

    QString errorMsg;
    if (!m_projectController->save(currentPath, &errorMsg)) {
        QMessageBox::critical(this, tr("KEY_MSG_SAVE_ERROR"), errorMsg);
    }
}

#include "widgets/exportdialog.h"

void MainWindow::on_actionExport_triggered()
{
    if (!m_projectController || !m_document || m_document->isEmpty()) {
        QMessageBox::warning(this, tr("KEY_ACTION_EXPORT"), tr("KEY_MSG_NOTHING_TO_EXPORT"));
        return;
    }

    ExportDialog dlg(m_document, QString(), this, m_projectController.get());
    dlg.exec();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::zoomSliderChanged(int val)
{
    if (m_atlasController) {
        m_atlasController->setZoomFactor(static_cast<double>(val) / 100.0);
    }
}

void MainWindow::onMergeFrames(int sourceRow, int targetRow)
{
    if (!m_document) return;
    if (m_undoStack) {
        m_undoStack->push(new MergeFramesCommand(m_document, sourceRow, targetRow));
    } else {
        m_document->mergeFrames(sourceRow, targetRow);
    }
}

void MainWindow::on_framesList_customContextMenuRequested(const QPoint &pos)
{
    QModelIndex clickedIdx = ui->framesList->indexAt(pos);
    if (clickedIdx.isValid() && ui->framesList->selectionModel()) {
        if (!ui->framesList->selectionModel()->isSelected(clickedIdx)) {
            ui->framesList->selectionModel()->select(clickedIdx, QItemSelectionModel::ClearAndSelect);
        }
    }

    QList<int> selectedIndices;
    if (ui->framesList->selectionModel()) {
        for (const QModelIndex &idx : ui->framesList->selectionModel()->selectedRows()) {
            selectedIndices.append(idx.row());
        }
    }
    if (selectedIndices.isEmpty() && clickedIdx.isValid()) {
        selectedIndices.append(clickedIdx.row());
    }

    QMenu menu(this);

    // 1. Create animation from selection
    QAction *createAnimAction = menu.addAction(tr("KEY_CTX_CREATE_ANIM"));
    createAnimAction->setEnabled(m_document && !selectedIndices.isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this, selectedIndices]() {
        if (m_animationController) {
            m_animationController->createAnimationFromSelection(selectedIndices);
        }
    });

    // 2. Add to existing animation submenu
    QString addToAnimTitle = tr("KEY_CTX_ADD_TO_ANIM");
    if (addToAnimTitle == QLatin1String("KEY_CTX_ADD_TO_ANIM")) {
        addToAnimTitle = QStringLiteral("Add to Animation");
    }
    QMenu *addToAnimMenu = menu.addMenu(addToAnimTitle);
    addToAnimMenu->setEnabled(m_document && !selectedIndices.isEmpty());

    QString activeAnim = m_animationController ? m_animationController->currentAnimationName() : QString();
    bool hasActive = !activeAnim.isEmpty() && activeAnim != QLatin1String("current") && m_document && m_document->hasAnimation(activeAnim);

    if (hasActive) {
        QString activeLabel = tr("KEY_CTX_ADD_TO_ACTIVE_ANIM");
        if (!activeLabel.contains(QLatin1String("%1"))) {
            activeLabel = QStringLiteral("Add to Active Animation '%1'");
        }
        QAction *actAddActive = addToAnimMenu->addAction(activeLabel.arg(activeAnim));
        connect(actAddActive, &QAction::triggered, this, [this, activeAnim, selectedIndices]() {
            if (m_animationController) {
                m_animationController->addFramesToAnimation(activeAnim, selectedIndices);
            }
        });
        addToAnimMenu->addSeparator();
    }

    QStringList animNames;
    if (m_document) {
        for (const QString &name : m_document->animations().keys()) {
            if (name != QLatin1String("current")) {
                animNames.append(name);
            }
        }
    }

    if (animNames.isEmpty()) {
        QString noAnimLabel = tr("KEY_CTX_NO_EXISTING_ANIMS");
        if (noAnimLabel == QLatin1String("KEY_CTX_NO_EXISTING_ANIMS")) {
            noAnimLabel = QStringLiteral("(No animations created yet)");
        }
        QAction *noAnimAct = addToAnimMenu->addAction(noAnimLabel);
        noAnimAct->setEnabled(false);
    } else {
        for (const QString &name : animNames) {
            if (hasActive && name == activeAnim) {
                continue; // Already added as active action above
            }
            const SpriteAnimation &anim = m_document->animation(name);
            QString label = QStringLiteral("%1 (%2 frames)").arg(name).arg(anim.frameIndices.size());
            QAction *act = addToAnimMenu->addAction(label);
            connect(act, &QAction::triggered, this, [this, name, selectedIndices]() {
                if (m_animationController) {
                    m_animationController->addFramesToAnimation(name, selectedIndices);
                }
            });
        }
    }

    if (selectedIndices.size() >= 2) {
        QAction *mergeAction = menu.addAction(tr("KEY_CTX_MERGE_SLICES") + "\tCtrl+Shift+M");
        connect(mergeAction, &QAction::triggered, this, [this]() {
            if (m_atlasController) {
                m_atlasController->mergeSelectedSlices();
            }
        });
    }

    menu.addSeparator();

    // 3. Edit in Pixel Editor
    QAction *editPixelsAction = menu.addAction(tr("KEY_ACTION_PIXEL_EDITOR") + "\tCtrl+E");
    editPixelsAction->setEnabled(clickedIdx.isValid());
    connect(editPixelsAction, &QAction::triggered, this, [this, clickedIdx]() {
        if (clickedIdx.isValid()) {
            openPixelEditorDialog(clickedIdx.row());
        }
    });

    menu.addSeparator();

    // 4. Select All & Invert
    QString selectAllLabel = tr("KEY_CTX_SELECT_ALL");
    if (selectAllLabel == QLatin1String("KEY_CTX_SELECT_ALL")) {
        selectAllLabel = QStringLiteral("Select All");
    }
    QAction *selectAllAction = menu.addAction(selectAllLabel + "\tCtrl+A");
    selectAllAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) {
            m_atlasController->selectAll();
        }
    });

    QAction *invertAction = menu.addAction(tr("KEY_CTX_INVERT_SEL"));
    invertAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(invertAction, &QAction::triggered, this, &MainWindow::invertSelection);

    menu.addSeparator();

    // 5. Erase Pixels
    QAction *eraseFramesAction = menu.addAction(tr("KEY_CTX_ERASE_PIXELS") + "\tShift+Del");
    eraseFramesAction->setEnabled(!selectedIndices.isEmpty());
    connect(eraseFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) {
            m_atlasController->eraseSelectedSlicesPixels();
        }
    });

    // 6. Delete Frames
    QAction *deleteFramesAction = menu.addAction(tr("KEY_CTX_DELETE_FRAMES") + "\tDel");
    deleteFramesAction->setEnabled(!selectedIndices.isEmpty());
    connect(deleteFramesAction, &QAction::triggered, this, &MainWindow::deleteSelectedFrame);

    menu.exec(ui->framesList->mapToGlobal(pos));
}

void MainWindow::deleteSelectedFrame()
{
    if (m_atlasController) {
        m_atlasController->deleteSelectedSlices();
    }
}

void MainWindow::invertSelection()
{
    if (m_atlasController) {
        m_atlasController->invertSelection();
    }
}
