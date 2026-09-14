#ifndef ANIMATIONCONTROLLER_H
#define ANIMATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTreeWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include "model/spritedocument.h"

class AnimationPlayer;
class TimelineFilmstripWidget;
class QUndoStack;

/**
 * @brief Controller managing animation playback, preview rendering, timeline, and animation list CRUD.
 */
class AnimationController : public QObject
{
    Q_OBJECT

public:
    explicit AnimationController(SpriteDocument *document,
                                 QUndoStack *undoStack = nullptr,
                                 AnimationPlayer *player = nullptr,
                                 QTreeWidget *treeWidget = nullptr,
                                 QGraphicsView *previewView = nullptr,
                                 QObject *parent = nullptr);
    ~AnimationController() override;

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    void stepForward();
    void stepBackward();
    void firstFrame();
    void lastFrame();
    void seek(int sequenceIndex);
    bool isPlaying() const;

    int fps() const;
    void setFps(int fps);

    SpriteAnimation::LoopMode loopMode() const;
    void setLoopMode(SpriteAnimation::LoopMode mode);

    QString currentAnimationName() const { return m_currentAnimationName; }
    int currentSequenceIndex() const;
    int currentGlobalFrameIndex() const;

    // Animation CRUD & Selection
    void selectAnimation(const QString &name);
    void createAnimation(const QString &name, const QList<int> &frameIndices, int fps = -1, SpriteAnimation::LoopMode loopMode = SpriteAnimation::Loop);
    void createAnimationFromSelection(const QList<int> &selectedIndices);
    void promptCreateNewAnimation();
    void removeAnimation(const QString &name);
    void removeAnimations(const QStringList &names);
    void removeSelectedAnimation();
    void renameAnimation(const QString &oldName, const QString &newName);
    void duplicateAnimation(const QString &sourceName);
    void duplicateSelectedAnimation();
    void reverseAnimationOrder();

    // Sequence manipulation
    void reorderAnimationFrames(const QString &name, const QList<int> &newSequence);
    void addFrameToAnimation(const QString &name, int globalIndex);
    void removeFrameFromAnimation(const QString &name, int seqIndex);
    void duplicateFrameInAnimation(const QString &name, int seqIndex);

    // Transient "current" selection animation
    void updateCurrentAnimation(const QList<int> &selectedIndices);
    void removeCurrentAnimation();
    bool hasCurrentAnimation() const;

    // View & UI sync
    void syncAnimationList();
    void updatePreview();
    void attachTreeWidget(QTreeWidget *treeWidget);
    void attachPreviewView(QGraphicsView *previewView);
    void attachTimelineWidget(TimelineFilmstripWidget *timeline);
    void attachScrubberSlider(QSlider *slider, QLabel *frameIndicator = nullptr);
    void attachLoopModeComboBox(QComboBox *combo);

    AnimationPlayer* player() const { return m_player; }
    QGraphicsScene* previewScene() const { return m_previewScene; }
    TimelineFilmstripWidget* timelineWidget() const { return m_timelineWidget; }

signals:
    void playbackStateChanged(bool isPlaying);
    void frameChanged(int sequenceIndex, int globalIndex);
    void fpsChanged(int fps);
    void loopModeChanged(SpriteAnimation::LoopMode mode);
    void currentAnimationChanged(const QString &name);
    void animationListChanged();
    void statusMessage(const QString &message);
    void framesSelectedInAnimation(const QList<int> &frameIndices);

private slots:
    void onPlayerFrameChanged(int seqIndex, int globalIndex);
    void onPlayerPlaybackStateChanged(bool playing);
    void onTreeItemSelectionChanged();
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onScrubberValueChanged(int value);

private:
    void renderCurrentFrame(int globalFrameIndex);
    void updateScrubberState();

    SpriteDocument          *m_document = nullptr;
    QUndoStack              *m_undoStack = nullptr;
    AnimationPlayer         *m_player = nullptr;
    bool                     m_ownsPlayer = false;
    QTreeWidget             *m_treeWidget = nullptr;
    QGraphicsView           *m_previewView = nullptr;
    QGraphicsScene          *m_previewScene = nullptr;
    QGraphicsPixmapItem     *m_previewPixmapItem = nullptr;
    TimelineFilmstripWidget *m_timelineWidget = nullptr;
    QSlider                 *m_scrubberSlider = nullptr;
    QLabel                  *m_frameIndicator = nullptr;
    QComboBox               *m_loopModeCombo = nullptr;
    QString                  m_currentAnimationName;
    bool                     m_isSyncingUi = false;
};

#endif // ANIMATIONCONTROLLER_H
