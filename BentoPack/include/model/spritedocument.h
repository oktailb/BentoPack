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

#ifndef SPRITEDOCUMENT_H
#define SPRITEDOCUMENT_H

#include <QObject>
#include <QImage>
#include <QList>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QPolygonF>
#include <QPointF>
#include "bentopackcore_export.h"

enum class PivotPreset {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Custom
};

/**
 * @brief Structure representing the bounding box of a sprite frame in the atlas.
 */
struct SPRITESTUDIO_CORE_EXPORT SpriteBox {
    QRect       rect;
    bool        selected = false;
    int         index = 0;
    int         groupId = 0;
    QList<int>  overlappingBoxes;
    QPoint      pivot = QPoint(0, 0); // Local position relative to the box's top-left corner
    bool        hasCustomPivot = false;

    // M8: 2D Polygon Mesh (local coordinates relative to rect.topLeft())
    QPolygonF       polygon;
    QList<QPointF>  vertices;
    QList<int>      triangles;
    bool            hasPolygonMesh = false;

    double polygonArea() const;
    double overdrawSavings() const;

    QPoint effectivePivot() const {
        return hasCustomPivot ? pivot : QPoint(rect.width() / 2, rect.height());
    }

    static QPoint calculatePresetPivot(PivotPreset preset, const QSize &size);

    SpriteBox() = default;
    explicit SpriteBox(const QRect &r) : rect(r) {}

    bool operator==(const SpriteBox &other) const {
        return rect == other.rect && index == other.index && selected == other.selected
               && pivot == other.pivot && hasCustomPivot == other.hasCustomPivot
               && hasPolygonMesh == other.hasPolygonMesh && polygon == other.polygon
               && triangles == other.triangles;
    }

    bool operator!=(const SpriteBox &other) const {
        return !(*this == other);
    }
};

/**
 * @brief Structure representing an animation sequence.
 */
struct SPRITESTUDIO_CORE_EXPORT SpriteAnimation {
    enum LoopMode {
        Loop = 0,
        Once = 1,
        PingPong = 2
    };

    QString     name;
    QList<int>  frameIndices;
    int         fps = 12;
    bool        loop = true;
    LoopMode    loopMode = Loop;

    int durationMs() const {
        if (fps <= 0 || frameIndices.isEmpty()) return 0;
        return (frameIndices.size() * 1000) / fps;
    }

    bool operator==(const SpriteAnimation &other) const {
        return name == other.name && frameIndices == other.frameIndices
               && fps == other.fps && loopMode == other.loopMode;
    }

    bool operator!=(const SpriteAnimation &other) const {
        return !(*this == other);
    }
};

/**
 * @brief SpriteDocument represents the central data model for a BentoPack project.
 *
 * It manages the raw atlas image, individual sliced frames, bounding boxes,
 * and named animations. It emits signals whenever the document content changes.
 */
class SPRITESTUDIO_CORE_EXPORT SpriteDocument : public QObject
{
    Q_OBJECT

public:
    explicit SpriteDocument(QObject *parent = nullptr);
    ~SpriteDocument() override = default;

    // Reset / Initialization
    void clear();
    bool isEmpty() const { return m_frames.isEmpty() && m_atlas.isNull(); }

    // File path & project info
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) {
        m_filePath = path;
        m_filePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    }
    QString projectName() const;

    // Atlas
    const QImage& atlas() const { return m_atlas; }
    void setAtlas(const QImage &image);
    void patchAtlas(const QRect &rect, const QImage &patch);
    void clearAtlasRegion(const QRect &rect);

    // Frames
    int frameCount() const { return m_frames.size(); }
    const QList<QImage>& frames() const { return m_frames; }
    QImage frame(int index) const;
    QImage polygonClippedFrame(int index) const;
    void setFrames(const QList<QImage> &frames, const QList<SpriteBox> &boxes);
    void addFrame(const QImage &image, const SpriteBox &box = SpriteBox());
    void insertFrame(int index, const QImage &image, const SpriteBox &box);
    void replaceFrame(int index, const QImage &image, const SpriteBox &box = SpriteBox());
    void removeFrame(int index);
    void removeFrames(const QList<int> &indices);
    void reorderFrames(const QList<int> &newOrder);
    void mergeFrames(int sourceIndex, int targetIndex);

    // Bounding Boxes
    const QList<SpriteBox>& boxes() const { return m_boxes; }
    SpriteBox box(int index) const;
    void setBox(int index, const SpriteBox &box);
    void updateBoxRect(int index, const QRect &newRect);
    int addSlice(const QRect &rect);
    QRect computeTrimmedRect(int index, int alphaThreshold = 1) const;
    void setBoxSelection(int index, bool selected);
    void setFrameSelected(int index, bool selected) { setBoxSelection(index, selected); }
    void clearBoxSelections();
    QList<int> selectedFrameIndices() const;
    void setSelectedFrameIndices(const QList<int> &indices);

    // Pivots & Origins
    QPoint boxPivot(int index) const;
    bool boxHasCustomPivot(int index) const;
    void setBoxPivot(int index, const QPoint &pivot, bool custom = true);
    void setBoxesPivot(const QList<int> &indices, const QPoint &pivot, bool custom = true);
    void applyPivotPreset(const QList<int> &indices, PivotPreset preset);
    QRect computeAnimationEnvelope(const QString &animName) const;

    // Dimensions
    int maxFrameWidth() const { return m_maxFrameWidth; }
    int maxFrameHeight() const { return m_maxFrameHeight; }

    // Animations
    QStringList animationNames() const { return m_animations.keys(); }
    bool hasAnimation(const QString &name) const { return m_animations.contains(name); }
    SpriteAnimation animation(const QString &name) const;
    const QMap<QString, SpriteAnimation>& animations() const { return m_animations; }
    void setAnimations(const QMap<QString, SpriteAnimation> &animations);
    void setAnimation(const QString &name, const QList<int> &frameIndices, int fps = 12, bool loop = true, SpriteAnimation::LoopMode loopMode = SpriteAnimation::Loop);
    void addAnimation(const QString &name, const QList<int> &frameIndices, int fps = 12, SpriteAnimation::LoopMode loopMode = SpriteAnimation::Loop)
    {
        setAnimation(name, frameIndices, fps, loopMode == SpriteAnimation::Loop, loopMode);
    }
    void setAnimation(const SpriteAnimation &anim);
    void removeAnimation(const QString &name);
    void renameAnimation(const QString &oldName, const QString &newName);
    void duplicateAnimation(const QString &sourceName, const QString &newName);
    void reverseAnimationFrames(const QString &name);
    void setAnimationLoopMode(const QString &name, SpriteAnimation::LoopMode mode);
    void setAnimationFrameSequence(const QString &name, const QList<int> &frameIndices);
    void insertFrameInAnimation(const QString &name, int seqIndex, int globalFrameIndex);
    void removeFrameFromAnimation(const QString &name, int seqIndex);

    // Manipulation helper
    void clearAtlasAreas(const QList<int> &frameIndices);

signals:
    void atlasChanged();
    void atlasRegionChanged(const QRect &rect);
    void framesChanged();
    void frameUpdated(int index);
    void boxPivotChanged(int index, const QPoint &pivot);
    void animationsChanged();
    void documentReset();

private:
    void recalculateMaxFrameDimensions();

    QImage                          m_atlas;
    QList<QImage>                   m_frames;
    QList<SpriteBox>                m_boxes;
    QList<int>                      m_selectedFrameIndices;
    QMap<QString, SpriteAnimation>  m_animations;
    QString                         m_filePath;
    int                             m_maxFrameWidth = 0;
    int                             m_maxFrameHeight = 0;
};

#endif // SPRITEDOCUMENT_H
