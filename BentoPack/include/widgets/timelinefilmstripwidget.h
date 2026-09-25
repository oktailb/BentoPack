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

#ifndef TIMELINEFILMSTRIPWIDGET_H
#define TIMELINEFILMSTRIPWIDGET_H

#include "bentopackcore_export.h"
#include "widgets/filmstriplistwidget.h"
#include <QWidget>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "model/spritedocument.h"

class AnimationController;

/**
 * @brief Interactive horizontal filmstrip timeline widget displaying and reordering the active animation's frames.
 */
class SPRITESTUDIO_CORE_EXPORT TimelineFilmstripWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineFilmstripWidget(QWidget *parent = nullptr);
    ~TimelineFilmstripWidget() override = default;

    void setDocument(SpriteDocument *document);
    void setAnimation(const QString &animationName);
    void setActiveSequenceIndex(int seqIndex);
    void retranslateUi();

    FilmstripListWidget *listWidget() const { return m_listWidget; }

protected:
    void changeEvent(QEvent *event) override;

signals:
    void frameSeekRequested(int seqIndex);
    void sequenceReordered(const QString &animName, const QList<int> &newSequence);
    void addFrameRequested(const QString &animName, int globalFrameIndex);
    void removeFrameRequested(const QString &animName, int seqIndex);
    void duplicateFrameRequested(const QString &animName, int seqIndex);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onCustomContextMenuRequested(const QPoint &pos);
    void onItemMoved(int fromIndex, int toIndex);

public slots:
    void refresh();

private:
    void setupUi();
    void rebuildItems();

    SpriteDocument      *m_document = nullptr;
    QString              m_animationName;
    int                  m_activeSeqIndex = -1;
    bool                 m_isRebuilding = false;
    bool                 m_isInternalReordering = false;

    QLabel              *m_lblTitle = nullptr;
    QLabel              *m_lblDuration = nullptr;
    FilmstripListWidget *m_listWidget = nullptr;
    QToolButton         *m_btnAddSelection = nullptr;
};

#endif // TIMELINEFILMSTRIPWIDGET_H
