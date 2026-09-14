#ifndef ANIMATIONPLAYER_H
#define ANIMATIONPLAYER_H

#include <QObject>
#include <QTimer>
#include <QList>

/**
 * @brief Autonomous animation controller managing playback timing and frame progression.
 */
class AnimationPlayer : public QObject
{
    Q_OBJECT

public:
    enum LoopMode {
        Loop = 0,
        Once = 1,
        PingPong = 2
    };

    explicit AnimationPlayer(QObject *parent = nullptr);
    ~AnimationPlayer() override = default;

    // Sequence setup
    void setSequence(const QList<int> &frameIndices, int fps = 12, LoopMode loopMode = Loop);
    void setSequence(const QList<int> &frameIndices, int fps, bool loop);
    const QList<int>& sequence() const { return m_frameIndices; }
    void clear();

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // Stepping & Navigation
    void advanceFrame();
    void stepForward();
    void stepBackward();
    void firstFrame();
    void lastFrame();
    void seek(int sequenceIndex);

    // Settings
    void setFps(int fps);
    int fps() const { return m_fps; }

    void setLoopMode(LoopMode mode);
    LoopMode loopMode() const { return m_loopMode; }

    void setLoop(bool loop) { setLoopMode(loop ? Loop : Once); }
    bool isLooping() const { return m_loopMode == Loop; }

    bool isPlaying() const { return m_isPlaying; }
    int currentSequenceIndex() const { return m_currentIndex; }
    int currentGlobalFrameIndex() const;
    int frameCount() const { return m_frameIndices.size(); }

signals:
    void frameChanged(int sequenceIndex, int globalFrameIndex);
    void playbackStateChanged(bool isPlaying);
    void loopModeChanged(LoopMode mode);

private slots:
    void onTick();

private:
    QTimer      m_timer;
    QList<int>  m_frameIndices;
    int         m_currentIndex = 0;
    int         m_fps = 12;
    LoopMode    m_loopMode = Loop;
    bool        m_pingPongForward = true;
    bool        m_isPlaying = false;
};

#endif // ANIMATIONPLAYER_H
