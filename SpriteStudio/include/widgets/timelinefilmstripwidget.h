#ifndef TIMELINEFILMSTRIPWIDGET_H
#define TIMELINEFILMSTRIPWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "model/spritedocument.h"

class AnimationController;

/**
 * @brief Interactive horizontal filmstrip timeline widget displaying and reordering the active animation's frames.
 */
class TimelineFilmstripWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineFilmstripWidget(QWidget *parent = nullptr);
    ~TimelineFilmstripWidget() override = default;

    void setDocument(SpriteDocument *document);
    void setAnimation(const QString &animationName);
    void setActiveSequenceIndex(int seqIndex);
    void retranslateUi();

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
    void onRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
                     const QModelIndex &destinationParent, int destinationRow);

public slots:
    void refresh();

private:
    void setupUi();
    void rebuildItems();

    SpriteDocument      *m_document = nullptr;
    QString              m_animationName;
    int                  m_activeSeqIndex = -1;
    bool                 m_isRebuilding = false;

    QLabel              *m_lblTitle = nullptr;
    QLabel              *m_lblDuration = nullptr;
    QListWidget         *m_listWidget = nullptr;
    QToolButton         *m_btnAddSelection = nullptr;
};

#endif // TIMELINEFILMSTRIPWIDGET_H
