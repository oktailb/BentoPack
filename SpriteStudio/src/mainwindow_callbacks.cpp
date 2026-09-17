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

    QString filter = tr("SpriteStudio Project (*.ssp);;All Files (*.*)");
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
        ? QDir::homePath() + QStringLiteral("/project.ssp")
        : m_projectController->currentProjectPath();

    QString filter = tr("SpriteStudio Project (*.ssp);;All Files (*.*)");
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

    ExportDialog dlg(m_document, QString(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QString selectedFile = dlg.exportFilePath();
    if (selectedFile.isEmpty()) return;

    ExportOptions options = dlg.exportOptions();
    QString errorMsg;
    if (!m_projectController->exportData(selectedFile, options, &errorMsg)) {
        QMessageBox::critical(this, tr("KEY_MSG_EXPORT_ERROR"), errorMsg);
    }
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

    QMenu menu(this);

    QAction *createAnimAction = menu.addAction(tr("KEY_CTX_CREATE_ANIM"));
    createAnimAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(createAnimAction, &QAction::triggered, this, [this]() {
        if (m_animationController && m_document) {
            m_animationController->createAnimationFromSelection(m_document->selectedFrameIndices());
        }
    });

    menu.addSeparator();

    QAction *deleteFramesAction = menu.addAction(tr("KEY_CTX_DELETE_FRAMES") + "\tDel");
    deleteFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(deleteFramesAction, &QAction::triggered, this, &MainWindow::deleteSelectedFrame);

    QAction *eraseFramesAction = menu.addAction(tr("KEY_CTX_ERASE_PIXELS") + "\tShift+Del");
    eraseFramesAction->setEnabled(m_document && !m_document->selectedFrameIndices().isEmpty());
    connect(eraseFramesAction, &QAction::triggered, this, [this]() {
        if (m_atlasController) {
            m_atlasController->eraseSelectedSlicesPixels();
        }
    });

    menu.addSeparator();

    QAction *invertAction = menu.addAction(tr("KEY_CTX_INVERT_SEL"));
    invertAction->setEnabled(m_document && m_document->frameCount() > 0);
    connect(invertAction, &QAction::triggered, this, &MainWindow::invertSelection);

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
