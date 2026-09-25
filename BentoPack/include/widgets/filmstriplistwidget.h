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

#ifndef FILMSTRIPLISTWIDGET_H
#define FILMSTRIPLISTWIDGET_H

#include "bentopackwidgets_export.h"
#include <QListWidget>
#include <QPoint>

class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QPaintEvent;
class QMouseEvent;

/**
 * @brief Customized horizontal QListWidget designed for sequential frame reordering.
 *
 * Implements deterministic linear drag-and-drop insertion with a high-contrast
 * vertical insertion line indicator, bypassing Qt's fragile IconMode InternalMove positioning.
 */
class BENTOPACK_WIDGETS_EXPORT FilmstripListWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit FilmstripListWidget(QWidget *parent = nullptr);
    ~FilmstripListWidget() override = default;

    /**
     * @brief Computes the drop insertion index [0, count()] from viewport coordinates.
     */
    int calculateDropIndex(const QPoint &pos) const;

    /**
     * @brief Moves an item from fromRow to toRow (insertion position [0, count()]).
     */
    void executeItemMove(int fromRow, int toRow);

signals:
    /**
     * @brief Emitted when an item was successfully moved from fromIndex to toIndex.
     */
    void itemMoved(int fromIndex, int toIndex);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint m_dragStartPos;
    int    m_dragSourceRow = -1;
    int    m_dropIndicatorIndex = -1;
};

#endif // FILMSTRIPLISTWIDGET_H
