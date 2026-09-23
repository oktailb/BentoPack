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

#include "widgets/filmstriplistwidget.h"
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygon>

static const char *s_filmstripMimeType = "application/x-spritestudio-filmstrip-item";

FilmstripListWidget::FilmstripListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setIconSize(QSize(64, 64));
    setSpacing(4);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMinimumHeight(100);

    // Enable custom drag-and-drop handling
    setDragEnabled(false); // We handle startDrag manually in mouseMoveEvent for full control
    setAcceptDrops(true);
    setDropIndicatorShown(false); // We draw our custom indicator in paintEvent

    setStyleSheet(QStringLiteral(
        "QListWidget::item {"
        "  border: 1px solid #c0c0c0;"
        "  border-radius: 4px;"
        "  padding: 2px;"
        "  background: #fafafa;"
        "}"
        "QListWidget::item:selected {"
        "  border: 2px solid #2980b9;"
        "  background: #e8f4fc;"
        "  color: #2980b9;"
        "  font-weight: bold;"
        "}"
    ));
}

void FilmstripListWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        QListWidgetItem *it = itemAt(event->pos());
        m_dragSourceRow = it ? row(it) : -1;
    }
    QListWidget::mousePressEvent(event);
}

void FilmstripListWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_dragSourceRow < 0) {
        QListWidget::mouseMoveEvent(event);
        return;
    }

    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QListWidget::mouseMoveEvent(event);
        return;
    }

    QListWidgetItem *it = item(m_dragSourceRow);
    if (!it) {
        QListWidget::mouseMoveEvent(event);
        return;
    }

    // Launch custom drag
    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(s_filmstripMimeType), QByteArray::number(m_dragSourceRow));
    drag->setMimeData(mimeData);

    QPixmap pix = it->icon().pixmap(QSize(48, 48));
    if (!pix.isNull()) {
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
    }

    m_dropIndicatorIndex = -1;
    viewport()->update();

    drag->exec(Qt::MoveAction);

    m_dropIndicatorIndex = -1;
    m_dragSourceRow = -1;
    viewport()->update();
}

void FilmstripListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(s_filmstripMimeType))) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FilmstripListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(s_filmstripMimeType))) {
        int idx = calculateDropIndex(event->position().toPoint());
        if (idx != m_dropIndicatorIndex) {
            m_dropIndicatorIndex = idx;
            viewport()->update();
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FilmstripListWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_dropIndicatorIndex = -1;
    viewport()->update();
    event->accept();
}

void FilmstripListWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(s_filmstripMimeType))) {
        int fromRow = event->mimeData()->data(QString::fromLatin1(s_filmstripMimeType)).toInt();
        int toRow = calculateDropIndex(event->position().toPoint());

        m_dropIndicatorIndex = -1;
        viewport()->update();
        event->acceptProposedAction();

        if (fromRow >= 0 && fromRow < count()) {
            executeItemMove(fromRow, toRow);
        }
    } else {
        event->ignore();
    }
}

int FilmstripListWidget::calculateDropIndex(const QPoint &pos) const
{
    int n = count();
    if (n == 0) return 0;

    // Check before first item
    QRect firstRect = visualItemRect(item(0));
    if (pos.x() <= firstRect.left()) {
        return 0;
    }

    // Check after last item
    QRect lastRect = visualItemRect(item(n - 1));
    if (pos.x() >= lastRect.right()) {
        return n;
    }

    // Check between/within items
    for (int i = 0; i < n; ++i) {
        QRect r = visualItemRect(item(i));
        int midX = r.center().x();
        if (pos.x() < midX) {
            return i;
        }
        if (i < n - 1) {
            QRect rNext = visualItemRect(item(i + 1));
            if (pos.x() < rNext.left()) {
                return i + 1;
            }
        }
    }

    return n;
}

void FilmstripListWidget::executeItemMove(int fromRow, int toRow)
{
    if (fromRow < 0 || fromRow >= count()) return;
    if (toRow < 0) toRow = 0;
    if (toRow > count()) toRow = count();

    // In linear insertion: if moving right, the removal of fromRow shifts indices down by 1
    int targetRow = (toRow > fromRow) ? (toRow - 1) : toRow;

    if (targetRow == fromRow) {
        return; // No-op
    }

    QListWidgetItem *it = takeItem(fromRow);
    if (!it) return;

    insertItem(targetRow, it);
    setCurrentItem(it);

    emit itemMoved(fromRow, targetRow);
}

void FilmstripListWidget::paintEvent(QPaintEvent *event)
{
    QListWidget::paintEvent(event);

    if (m_dropIndicatorIndex >= 0) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);

        int n = count();
        int x = 4;
        if (n == 0) {
            x = 4;
        } else if (m_dropIndicatorIndex == 0) {
            x = qMax(2, visualItemRect(item(0)).left() - 3);
        } else if (m_dropIndicatorIndex >= n) {
            x = visualItemRect(item(n - 1)).right() + 3;
        } else {
            QRect rPrev = visualItemRect(item(m_dropIndicatorIndex - 1));
            QRect rCurr = visualItemRect(item(m_dropIndicatorIndex));
            x = (rPrev.right() + rCurr.left()) / 2;
        }

        QColor indicatorColor(41, 128, 185); // Professional Qt blue (#2980b9)
        QPen pen(indicatorColor, 3, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(indicatorColor);

        int top = 8;
        int bottom = viewport()->height() - 8;
        painter.drawLine(x, top, x, bottom);

        // Top triangle cap
        QPolygon topTriangle;
        topTriangle << QPoint(x - 5, top) << QPoint(x + 5, top) << QPoint(x, top + 6);
        painter.drawPolygon(topTriangle);

        // Bottom triangle cap
        QPolygon bottomTriangle;
        bottomTriangle << QPoint(x - 5, bottom) << QPoint(x + 5, bottom) << QPoint(x, bottom - 6);
        painter.drawPolygon(bottomTriangle);
    }
}
