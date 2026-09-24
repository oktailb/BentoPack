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

#include "widgets/timelinefilmstripwidget.h"
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QEvent>

TimelineFilmstripWidget::TimelineFilmstripWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TimelineFilmstripWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    // Header toolbar
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(2, 0, 2, 0);
    headerLayout->setSpacing(8);

    m_lblTitle = new QLabel(tr("KEY_TIMELINE_TITLE"), this);
    headerLayout->addWidget(m_lblTitle);

    m_lblDuration = new QLabel(this);
    m_lblDuration->setStyleSheet(QStringLiteral("color: #7f8c8d; font-size: 11px;"));
    headerLayout->addWidget(m_lblDuration);

    headerLayout->addStretch();

    m_btnAddSelection = new QToolButton(this);
    m_btnAddSelection->setText(tr("KEY_TIMELINE_ADD_SELECTION"));
    m_btnAddSelection->setToolTip(tr("KEY_TIMELINE_ADD_SELECTION_TOOLTIP"));
    connect(m_btnAddSelection, &QToolButton::clicked, this, [this]() {
        if (!m_document || m_animationName.isEmpty()) return;
        for (int globalIdx : m_document->selectedFrameIndices()) {
            emit addFrameRequested(m_animationName, globalIdx);
        }
    });
    headerLayout->addWidget(m_btnAddSelection);

    mainLayout->addLayout(headerLayout);

    // List widget with deterministic drag-and-drop
    m_listWidget = new FilmstripListWidget(this);

    connect(m_listWidget, &FilmstripListWidget::itemClicked, this, &TimelineFilmstripWidget::onItemClicked);
    connect(m_listWidget, &FilmstripListWidget::customContextMenuRequested, this, &TimelineFilmstripWidget::onCustomContextMenuRequested);
    connect(m_listWidget, &FilmstripListWidget::itemMoved, this, &TimelineFilmstripWidget::onItemMoved);

    mainLayout->addWidget(m_listWidget);
}

void TimelineFilmstripWidget::setDocument(SpriteDocument *document)
{
    if (m_document == document) return;
    if (m_document) {
        disconnect(m_document, &SpriteDocument::framesChanged, this, &TimelineFilmstripWidget::refresh);
        disconnect(m_document, &SpriteDocument::frameUpdated, this, &TimelineFilmstripWidget::refresh);
        disconnect(m_document, &SpriteDocument::animationsChanged, this, &TimelineFilmstripWidget::refresh);
    }
    m_document = document;
    if (m_document) {
        connect(m_document, &SpriteDocument::framesChanged, this, &TimelineFilmstripWidget::refresh);
        connect(m_document, &SpriteDocument::frameUpdated, this, &TimelineFilmstripWidget::refresh);
        connect(m_document, &SpriteDocument::animationsChanged, this, &TimelineFilmstripWidget::refresh);
    }
    refresh();
}

void TimelineFilmstripWidget::setAnimation(const QString &animationName)
{
    if (m_isInternalReordering) return;
    m_animationName = animationName;
    m_activeSeqIndex = -1;
    refresh();
}

void TimelineFilmstripWidget::setActiveSequenceIndex(int seqIndex)
{
    m_activeSeqIndex = seqIndex;
    if (!m_listWidget || m_isRebuilding) return;

    if (seqIndex >= 0 && seqIndex < m_listWidget->count()) {
        m_listWidget->blockSignals(true);
        m_listWidget->setCurrentRow(seqIndex);
        m_listWidget->scrollToItem(m_listWidget->item(seqIndex), QAbstractItemView::EnsureVisible);
        m_listWidget->blockSignals(false);
    }
}

void TimelineFilmstripWidget::refresh()
{
    if (m_isInternalReordering) return;
    rebuildItems();
}

void TimelineFilmstripWidget::rebuildItems()
{
    if (!m_listWidget || m_isInternalReordering) return;

    if (!m_document || m_animationName.isEmpty() || !m_document->hasAnimation(m_animationName)) {
        m_isRebuilding = true;
        m_listWidget->clear();
        m_lblTitle->setText(tr("KEY_TIMELINE_NO_ANIM"));
        m_lblDuration->clear();
        m_btnAddSelection->setEnabled(false);
        m_isRebuilding = false;
        return;
    }

    m_btnAddSelection->setEnabled(true);
    const SpriteAnimation &anim = m_document->animation(m_animationName);
    QString titlePattern = tr("KEY_TIMELINE_ANIM_INFO");
    if (!titlePattern.contains(QLatin1String("%1"))) {
        titlePattern = QStringLiteral("<b>Timeline: %1</b> (%2 frames)");
    }
    m_lblTitle->setText(titlePattern.arg(anim.name).arg(anim.frameIndices.size()));

    int ms = anim.durationMs();
    QString durPattern = tr("KEY_TIMELINE_DURATION_FPS");
    if (!durPattern.contains(QLatin1String("%1"))) {
        durPattern = QStringLiteral("%1 ms @ %2 FPS");
    }
    m_lblDuration->setText(durPattern.arg(ms).arg(anim.fps));

    int frameDuration = anim.fps > 0 ? (1000 / anim.fps) : 100;

    // Fast-path check: verify if items match current animation sequence
    bool matches = (m_listWidget->count() == anim.frameIndices.size());
    if (matches) {
        for (int i = 0; i < anim.frameIndices.size(); ++i) {
            QListWidgetItem *it = m_listWidget->item(i);
            if (!it || it->data(Qt::UserRole).toInt() != anim.frameIndices.at(i)) {
                matches = false;
                break;
            }
        }
    }

    if (matches) {
        // Update thumbnail icons to reflect latest frame modifications (e.g. background removal, pixel edits)
        for (int seqIdx = 0; seqIdx < anim.frameIndices.size(); ++seqIdx) {
            int globalIdx = anim.frameIndices.at(seqIdx);
            QListWidgetItem *item = m_listWidget->item(seqIdx);
            if (item && m_document && globalIdx >= 0 && globalIdx < m_document->frameCount()) {
                QImage img = m_document->polygonClippedFrame(globalIdx);
                if (!img.isNull()) {
                    item->setIcon(QIcon(QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
                }
            }
        }
        if (m_activeSeqIndex >= 0 && m_activeSeqIndex < m_listWidget->count()) {
            m_listWidget->setCurrentRow(m_activeSeqIndex);
        }
        return;
    }

    m_isRebuilding = true;
    m_listWidget->clear();

    QString tipPattern = tr("KEY_TIMELINE_FRAME_TOOLTIP");
    if (!tipPattern.contains(QLatin1String("%1"))) {
        tipPattern = QStringLiteral("Step #%1: Global Frame %2 (%3 ms)\nDrag and drop to reorder");
    }

    for (int seqIdx = 0; seqIdx < anim.frameIndices.size(); ++seqIdx) {
        int globalIdx = anim.frameIndices.at(seqIdx);
        auto *item = new QListWidgetItem(m_listWidget);

        QImage img;
        if (globalIdx >= 0 && globalIdx < m_document->frameCount()) {
            img = m_document->polygonClippedFrame(globalIdx);
        }

        if (!img.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        }

        item->setText(tr("#%1 (F%2)").arg(seqIdx + 1).arg(globalIdx + 1));
        item->setToolTip(tipPattern.arg(seqIdx + 1).arg(globalIdx + 1).arg(frameDuration));
        item->setData(Qt::UserRole, globalIdx);
        item->setData(Qt::UserRole + 1, seqIdx);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        m_listWidget->addItem(item);
    }

    if (m_activeSeqIndex >= 0 && m_activeSeqIndex < m_listWidget->count()) {
        m_listWidget->setCurrentRow(m_activeSeqIndex);
    }

    m_isRebuilding = false;
}

void TimelineFilmstripWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    int seqIdx = m_listWidget->row(item);
    emit frameSeekRequested(seqIdx);
}

void TimelineFilmstripWidget::onItemMoved(int fromIndex, int toIndex)
{
    Q_UNUSED(fromIndex);
    if (m_isRebuilding || m_animationName.isEmpty() || !m_document) return;

    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item) {
            int globalIdx = item->data(Qt::UserRole).toInt();
            item->setText(tr("#%1 (F%2)").arg(i + 1).arg(globalIdx + 1));
            item->setData(Qt::UserRole + 1, i);
        }
    }

    QList<int> newSeq;
    newSeq.reserve(m_listWidget->count());
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item) {
            newSeq.append(item->data(Qt::UserRole).toInt());
        }
    }

    m_activeSeqIndex = toIndex;

    m_isInternalReordering = true;
    emit sequenceReordered(m_animationName, newSeq);
    m_isInternalReordering = false;

    emit frameSeekRequested(toIndex);
}

void TimelineFilmstripWidget::onCustomContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item) return;

    int seqIdx = m_listWidget->row(item);
    int globalIdx = item->data(Qt::UserRole).toInt();

    QMenu menu(this);
    QAction *actDuplicate = menu.addAction(tr("KEY_TIMELINE_DUPLICATE_FRAME"));
    QAction *actRemove = menu.addAction(tr("KEY_TIMELINE_REMOVE_FRAME"));
    menu.addSeparator();
    QAction *actSelectAtlas = menu.addAction(tr("KEY_TIMELINE_SELECT_IN_ATLAS"));

    QAction *chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actDuplicate) {
        emit duplicateFrameRequested(m_animationName, seqIdx);
    } else if (chosen == actRemove) {
        emit removeFrameRequested(m_animationName, seqIdx);
    } else if (chosen == actSelectAtlas && m_document) {
        m_document->clearBoxSelections();
        m_document->setFrameSelected(globalIdx, true);
    }
}

void TimelineFilmstripWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TimelineFilmstripWidget::retranslateUi()
{
    if (m_btnAddSelection) {
        m_btnAddSelection->setText(tr("KEY_TIMELINE_ADD_SELECTION"));
        m_btnAddSelection->setToolTip(tr("KEY_TIMELINE_ADD_SELECTION_TOOLTIP"));
    }
    rebuildItems();
}

