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
#include "include/config/appconfig.h"
#include "widgets/settingsdialog.h"
#include <QMouseEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QSettings>

#include <QLineEdit>
#include <QTextEdit>
#include <QAbstractSpinBox>

void MainWindow::wheelEvent(QWheelEvent *event)
{
    QMainWindow::wheelEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();

    // Universal Spacebar shortcut to Toggle Play / Pause anywhere in the app
    if (key == Qt::Key_Space) {
        QWidget *focused = focusWidget();
        if (!qobject_cast<QLineEdit*>(focused) &&
            !qobject_cast<QTextEdit*>(focused) &&
            !qobject_cast<QAbstractSpinBox*>(focused)) {
            if (m_animationController) {
                m_animationController->togglePlayPause();
                event->accept();
                return;
            }
        }
    }

    // Home/End: First / Last frame
    if (key == Qt::Key_Home) {
        if (m_animationController) {
            m_animationController->firstFrame();
            event->accept();
            return;
        }
    } else if (key == Qt::Key_End) {
        if (m_animationController) {
            m_animationController->lastFrame();
            event->accept();
            return;
        }
    }

    if (m_atlasController && m_document && !m_document->selectedFrameIndices().isEmpty()) {
        // Delete / Backspace: Check first if polygon mesh vertices are selected
        if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && !(event->modifiers() & Qt::ShiftModifier)) {
            if (m_atlasController->deleteSelectedMeshVertices()) {
                event->accept();
                return;
            }
        }

        // Shift + Delete: Erase pixels from atlas & delete slice
        if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && (event->modifiers() & Qt::ShiftModifier)) {
            m_atlasController->eraseSelectedSlicesPixels();
            event->accept();
            return;
        }

        // Delete without Shift: Delete slice only (non-destructive)
        if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
            m_atlasController->deleteSelectedSlices();
            event->accept();
            return;
        }

        // Arrow keys: Nudge selected vertices or boxes
        if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down) {
            const AtlasConfig &cfg = AppConfig::instance().atlas();
            int step = (event->modifiers() & Qt::ShiftModifier) ? cfg.nudgeStepLarge : cfg.nudgeStepSmall;
            int dx = 0;
            int dy = 0;
            if (key == Qt::Key_Left)  dx = -step;
            if (key == Qt::Key_Right) dx = step;
            if (key == Qt::Key_Up)    dy = -step;
            if (key == Qt::Key_Down)  dy = step;

            if (m_atlasController->nudgeSelectedMeshVertices(dx, dy)) {
                event->accept();
                return;
            }

            m_atlasController->nudgeSelectedBoxes(dx, dy);
            event->accept();
            return;
        }
    } else {
        // Left / Right arrows: Frame-by-frame stepping in animation player
        if (key == Qt::Key_Left) {
            if (m_animationController) {
                m_animationController->stepBackward();
                event->accept();
                return;
            }
        } else if (key == Qt::Key_Right) {
            if (m_animationController) {
                m_animationController->stepForward();
                event->accept();
                return;
            }
        }
    }
    QMainWindow::keyPressEvent(event);
}

#include <QFileInfo>
#include <QUrl>
#include <QFile>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QAbstractScrollArea>
#include <QChildEvent>

namespace {

QString extractFilePathFromMimeData(const QMimeData *mime)
{
    if (!mime) return QString();

    auto cleanPath = [](const QString &raw) -> QString {
        QString p = raw.trimmed();
        if (p.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
            QUrl u(p);
            p = u.toLocalFile();
            if (p.isEmpty()) {
                p = u.path();
            }
        }
        if (p.startsWith(QLatin1String("/"))) {
            p = QUrl::fromPercentEncoding(p.toUtf8());
        }
        return p;
    };

    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            if (path.isEmpty() && url.scheme() == QLatin1String("file")) {
                path = url.path();
            }
            path = cleanPath(path);
            if (!path.isEmpty() && QFile::exists(path)) {
                return path;
            }
        }
    }

    if (mime->hasFormat(QStringLiteral("text/uri-list"))) {
        QByteArray data = mime->data(QStringLiteral("text/uri-list"));
        QList<QByteArray> lines = data.split('\n');
        for (QByteArray line : lines) {
            line = line.trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            QString str = QString::fromUtf8(line);
            QString path = cleanPath(str);
            if (!path.isEmpty() && QFile::exists(path)) {
                return path;
            }
        }
    }

    if (mime->hasText()) {
        QString text = mime->text().trimmed();
        QString path = cleanPath(text);
        if (!path.isEmpty() && QFile::exists(path)) {
            return path;
        }
    }

    // Fallback if QFile::exists failed but URL was present
    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            if (path.isEmpty() && urls.first().scheme() == QLatin1String("file")) {
                path = urls.first().path();
            }
            path = cleanPath(path);
            if (!path.isEmpty()) {
                return path;
            }
        }
    }

    return QString();
}

bool hasDroppableFile(const QMimeData *mime)
{
    if (!mime) return false;
    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile() || url.scheme().isEmpty() || url.scheme() == QLatin1String("file")) {
                return true;
            }
        }
    }
    if (mime->hasFormat(QStringLiteral("text/uri-list"))) {
        return true;
    }
    if (mime->hasText()) {
        QString text = mime->text().trimmed();
        if (text.startsWith(QLatin1String("file://"), Qt::CaseInsensitive) ||
            (text.startsWith(QLatin1Char('/')) && text.length() < 1024)) {
            return true;
        }
    }
    return false;
}

} // namespace

void MainWindow::enableFileDropsRecursively(QWidget *widget)
{
    if (!widget) return;
    widget->setAcceptDrops(true);
    widget->removeEventFilter(this);
    widget->installEventFilter(this);

    if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(widget)) {
        if (QWidget *vp = scrollArea->viewport()) {
            vp->setAcceptDrops(true);
            vp->removeEventFilter(this);
            vp->installEventFilter(this);
        }
    }

    const auto childrenList = widget->children();
    for (QObject *child : childrenList) {
        if (QWidget *childWidget = qobject_cast<QWidget *>(child)) {
            enableFileDropsRecursively(childWidget);
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_vramGaugeWidget && event->type() == QEvent::MouseButtonRelease) {
        SettingsDialog dlg(this, SettingsDialog::PageRetro);
        dlg.exec();
        return true;
    }

    // Global file drag-and-drop handling for all child widgets, central widget and viewports
    if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent *de = static_cast<QDragEnterEvent *>(event);
        if (hasDroppableFile(de->mimeData())) {
            de->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::DragMove) {
        QDragMoveEvent *dme = static_cast<QDragMoveEvent *>(event);
        if (hasDroppableFile(dme->mimeData())) {
            dme->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::DragLeave) {
        // Let leave event pass through cleanly
    } else if (event->type() == QEvent::Drop) {
        QDropEvent *de = static_cast<QDropEvent *>(event);
        if (hasDroppableFile(de->mimeData())) {
            QString path = extractFilePathFromMimeData(de->mimeData());
            if (!path.isEmpty()) {
                de->acceptProposedAction();
                processFile(path);
                return true;
            }
        }
    } else if (event->type() == QEvent::ChildAdded) {
        QChildEvent *ce = static_cast<QChildEvent *>(event);
        if (ce && ce->child()) {
            if (QWidget *cw = qobject_cast<QWidget *>(ce->child())) {
                enableFileDropsRecursively(cw);
            }
        }
    }

    if (watched == ui->framesList || watched == ui->framesList->viewport()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *kEvent = static_cast<QKeyEvent *>(event);
            int key = kEvent->key();
            if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
                if (m_atlasController && m_document && !m_document->selectedFrameIndices().isEmpty()) {
                    if (kEvent->modifiers() & Qt::ShiftModifier) {
                        m_atlasController->eraseSelectedSlicesPixels();
                    } else {
                        m_atlasController->deleteSelectedSlices();
                    }
                    return true;
                }
            }
        }
    }

    if (watched == ui->framesList->viewport()) {
        if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dmEvent = static_cast<QDragMoveEvent *>(event);
            QPoint pos = dmEvent->position().toPoint();
            QModelIndex index = ui->framesList->indexAt(pos);

            ui->framesList->setDropIndicatorShown(false);

            if (index.isValid()) {
                QRect rect = ui->framesList->visualRect(index);
                int relativeX = pos.x() - rect.left();
                int width = rect.width();
                int margin = static_cast<int>(width * 0.2);

                if (relativeX < margin) {
                    listDelegate->setHighlight(index.row(), FrameDelegate::InsertLeft);
                } else if (relativeX > (width - margin)) {
                    listDelegate->setHighlight(index.row(), FrameDelegate::InsertRight);
                } else {
                    listDelegate->setHighlight(index.row(), FrameDelegate::Merge);
                }
            } else {
                listDelegate->setHighlight(-1, FrameDelegate::None);
            }

            ui->framesList->viewport()->update();
        } else if (event->type() == QEvent::DragLeave || event->type() == QEvent::Drop) {
            listDelegate->setHighlight(-1, FrameDelegate::None);
            ui->framesList->viewport()->update();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    saveLayoutState();

    if (maybeSave()) {
        e->accept();
    } else {
        e->ignore();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_atlasController) {
        m_atlasController->adjustZoomToWindow();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasDroppableFile(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QMainWindow::dragEnterEvent(event);
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (hasDroppableFile(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QMainWindow::dragMoveEvent(event);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    QMainWindow::dragLeaveEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QString localFile = extractFilePathFromMimeData(event->mimeData());
    if (!localFile.isEmpty()) {
        event->acceptProposedAction();
        processFile(localFile);
        return;
    }
    QMainWindow::dropEvent(event);
}
