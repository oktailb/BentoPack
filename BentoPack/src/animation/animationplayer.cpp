#include "animation/animationplayer.h"

AnimationPlayer::AnimationPlayer(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &AnimationPlayer::onTick);
}

void AnimationPlayer::setSequence(const QList<int> &frameIndices, int fps, LoopMode loopMode)
{
    m_frameIndices = frameIndices;
    m_fps = (fps > 0) ? fps : 12;
    m_loopMode = loopMode;
    m_currentIndex = 0;
    m_pingPongForward = true;

    if (m_timer.isActive()) {
        m_timer.setInterval(1000 / m_fps);
    }

    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::setSequence(const QList<int> &frameIndices, int fps, bool loop)
{
    setSequence(frameIndices, fps, loop ? Loop : Once);
}

void AnimationPlayer::setLoopMode(LoopMode mode)
{
    if (m_loopMode != mode) {
        m_loopMode = mode;
        m_pingPongForward = true;
        emit loopModeChanged(mode);
    }
}

void AnimationPlayer::clear()
{
    stop();
    m_frameIndices.clear();
    m_currentIndex = 0;
    m_pingPongForward = true;
    emit frameChanged(0, -1);
}

void AnimationPlayer::play()
{
    if (m_frameIndices.isEmpty()) return;

    m_isPlaying = true;
    m_timer.start(1000 / m_fps);
    emit playbackStateChanged(true);
}

void AnimationPlayer::pause()
{
    if (m_isPlaying) {
        m_isPlaying = false;
        m_timer.stop();
        emit playbackStateChanged(false);
    }
}

void AnimationPlayer::stop()
{
    m_isPlaying = false;
    m_timer.stop();
    m_currentIndex = 0;
    m_pingPongForward = true;
    emit playbackStateChanged(false);
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void AnimationPlayer::advanceFrame()
{
    if (m_frameIndices.isEmpty()) return;
    if (m_frameIndices.size() == 1) return;

    if (m_loopMode == Once) {
        if (m_currentIndex + 1 < m_frameIndices.size()) {
            m_currentIndex++;
        } else {
            pause();
            return;
        }
    } else if (m_loopMode == PingPong) {
        if (m_pingPongForward) {
            if (m_currentIndex + 1 < m_frameIndices.size()) {
                m_currentIndex++;
            } else {
                m_pingPongForward = false;
                m_currentIndex--;
            }
        } else {
            if (m_currentIndex > 0) {
                m_currentIndex--;
            } else {
                m_pingPongForward = true;
                m_currentIndex++;
            }
        }
    } else { // Loop
        m_currentIndex = (m_currentIndex + 1) % m_frameIndices.size();
    }

    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::stepForward()
{
    pause();
    advanceFrame();
}

void AnimationPlayer::stepBackward()
{
    if (m_frameIndices.isEmpty()) return;
    pause();
    if (m_frameIndices.size() == 1) return;

    if (m_loopMode == Once) {
        if (m_currentIndex > 0) {
            m_currentIndex--;
        }
    } else if (m_loopMode == PingPong) {
        if (!m_pingPongForward) {
            if (m_currentIndex + 1 < m_frameIndices.size()) {
                m_currentIndex++;
            } else {
                m_pingPongForward = true;
                m_currentIndex--;
            }
        } else {
            if (m_currentIndex > 0) {
                m_currentIndex--;
            } else {
                m_pingPongForward = false;
                m_currentIndex++;
            }
        }
    } else { // Loop
        m_currentIndex = (m_currentIndex - 1 + m_frameIndices.size()) % m_frameIndices.size();
    }

    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::firstFrame()
{
    if (m_frameIndices.isEmpty()) return;
    m_currentIndex = 0;
    m_pingPongForward = true;
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::lastFrame()
{
    if (m_frameIndices.isEmpty()) return;
    m_currentIndex = m_frameIndices.size() - 1;
    m_pingPongForward = false;
    emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
}

void AnimationPlayer::seek(int sequenceIndex)
{
    if (sequenceIndex >= 0 && sequenceIndex < m_frameIndices.size()) {
        m_currentIndex = sequenceIndex;
        emit frameChanged(m_currentIndex, currentGlobalFrameIndex());
    }
}

void AnimationPlayer::setFps(int fps)
{
    if (fps <= 0) return;
    m_fps = fps;
    if (m_timer.isActive()) {
        m_timer.setInterval(1000 / m_fps);
    }
}

int AnimationPlayer::currentGlobalFrameIndex() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_frameIndices.size()) {
        return m_frameIndices.at(m_currentIndex);
    }
    return -1;
}

void AnimationPlayer::onTick()
{
    if (m_frameIndices.isEmpty()) {
        stop();
        return;
    }
    advanceFrame();
}
