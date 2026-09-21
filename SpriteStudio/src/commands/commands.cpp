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

#include "commands/commands.h"
#include <algorithm>
#include <QPainter>

// --- DeleteFramesCommand ---

DeleteFramesCommand::DeleteFramesCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_indicesToDelete(indices)
{
    setText(QObject::tr("Delete %n frame(s)", "", indices.size()));

    // Sort ascending for backup recording
    std::sort(m_indicesToDelete.begin(), m_indicesToDelete.end());
    m_indicesToDelete.erase(std::unique(m_indicesToDelete.begin(), m_indicesToDelete.end()), m_indicesToDelete.end());

    for (int idx : m_indicesToDelete) {
        if (idx >= 0 && idx < m_doc->frameCount()) {
            FrameBackup fb;
            fb.originalIndex = idx;
            fb.image = m_doc->frame(idx);
            fb.box = m_doc->box(idx);
            m_deletedFrames.append(fb);
        }
    }

    m_animationsBackup = m_doc->animations();
}

void DeleteFramesCommand::redo()
{
    m_doc->removeFrames(m_indicesToDelete);
}

void DeleteFramesCommand::undo()
{
    // Reinsert deleted frames in ascending order
    for (const FrameBackup &fb : m_deletedFrames) {
        m_doc->insertFrame(fb.originalIndex, fb.image, fb.box);
    }

    // Restore exact animations state
    for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
        m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
    }
}

// --- EraseAtlasPixelsCommand ---

EraseAtlasPixelsCommand::EraseAtlasPixelsCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_indicesToDelete(indices)
{
    setText(QObject::tr("Erase Atlas Pixels for %n frame(s)", "", indices.size()));

    std::sort(m_indicesToDelete.begin(), m_indicesToDelete.end());
    m_indicesToDelete.erase(std::unique(m_indicesToDelete.begin(), m_indicesToDelete.end()), m_indicesToDelete.end());

    for (int idx : m_indicesToDelete) {
        if (idx >= 0 && idx < m_doc->frameCount()) {
            FrameBackup fb;
            fb.originalIndex = idx;
            fb.image = m_doc->frame(idx);
            fb.box = m_doc->box(idx);
            if (m_doc && !m_doc->atlas().isNull() && !fb.box.rect.isEmpty()) {
                QRect clamped = fb.box.rect.intersected(m_doc->atlas().rect());
                if (!clamped.isEmpty()) {
                    fb.atlasPatch = m_doc->atlas().copy(clamped);
                }
            }
            m_deletedFrames.append(fb);
        }
    }

    m_animationsBackup = m_doc ? m_doc->animations() : QMap<QString, SpriteAnimation>();
}

void EraseAtlasPixelsCommand::redo()
{
    if (m_doc) {
        for (const FrameBackup &fb : m_deletedFrames) {
            if (!fb.box.rect.isEmpty()) {
                m_doc->clearAtlasRegion(fb.box.rect);
            }
        }
        m_doc->removeFrames(m_indicesToDelete);
    }
}

void EraseAtlasPixelsCommand::undo()
{
    if (m_doc) {
        for (const FrameBackup &fb : m_deletedFrames) {
            if (!fb.box.rect.isEmpty() && !fb.atlasPatch.isNull()) {
                m_doc->patchAtlas(fb.box.rect, fb.atlasPatch);
            }
            m_doc->insertFrame(fb.originalIndex, fb.image, fb.box);
        }
        for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
            m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
        }
    }
}

// --- MergeFramesCommand ---

MergeFramesCommand::MergeFramesCommand(SpriteDocument *doc, int sourceIndex, int targetIndex, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_sourceIndex(sourceIndex)
    , m_targetIndex(targetIndex)
{
    setText(QObject::tr("Merge Frame %1 into %2").arg(sourceIndex + 1).arg(targetIndex + 1));

    m_sourceImage = m_doc->frame(sourceIndex);
    m_sourceBox = m_doc->box(sourceIndex);
    m_targetOriginalImage = m_doc->frame(targetIndex);
    m_targetOriginalBox = m_doc->box(targetIndex);
    m_animationsBackup = m_doc->animations();
}

void MergeFramesCommand::redo()
{
    m_doc->mergeFrames(m_sourceIndex, m_targetIndex);
}

void MergeFramesCommand::undo()
{
    // Restore target frame to its original pixmap and box
    int adjustedTarget = (m_sourceIndex < m_targetIndex) ? (m_targetIndex - 1) : m_targetIndex;
    if (adjustedTarget >= 0 && adjustedTarget < m_doc->frameCount()) {
        m_doc->removeFrame(adjustedTarget);
    }

    // Reinsert both original frames
    if (m_sourceIndex <= m_targetIndex) {
        m_doc->insertFrame(m_sourceIndex, m_sourceImage, m_sourceBox);
        m_doc->insertFrame(m_targetIndex, m_targetOriginalImage, m_targetOriginalBox);
    } else {
        m_doc->insertFrame(m_targetIndex, m_targetOriginalImage, m_targetOriginalBox);
        m_doc->insertFrame(m_sourceIndex, m_sourceImage, m_sourceBox);
    }

    // Restore animations
    for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
        m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
    }
}

// --- CreateAnimationCommand ---

CreateAnimationCommand::CreateAnimationCommand(SpriteDocument *doc, const QString &name, const QList<int> &frameIndices, int fps, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_name(name)
    , m_frameIndices(frameIndices)
    , m_fps(fps)
{
    setText(QObject::tr("Create Animation '%1'").arg(name));
}

void CreateAnimationCommand::redo()
{
    m_doc->setAnimation(m_name, m_frameIndices, m_fps, true);
}

void CreateAnimationCommand::undo()
{
    m_doc->removeAnimation(m_name);
}

// --- DeleteAnimationCommand ---

DeleteAnimationCommand::DeleteAnimationCommand(SpriteDocument *doc, const QString &name, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_name(name)
{
    setText(QObject::tr("Delete Animation '%1'").arg(name));
    m_backup = m_doc->animation(name);
}

void DeleteAnimationCommand::redo()
{
    m_doc->removeAnimation(m_name);
}

void DeleteAnimationCommand::undo()
{
    m_doc->setAnimation(m_backup);
}

// --- ReverseAnimationCommand ---

ReverseAnimationCommand::ReverseAnimationCommand(SpriteDocument *doc, const QString &animName, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_animName(animName)
{
    setText(QObject::tr("Reverse Animation '%1'").arg(animName));
}

void ReverseAnimationCommand::redo()
{
    m_doc->reverseAnimationFrames(m_animName);
}

void ReverseAnimationCommand::undo()
{
    m_doc->reverseAnimationFrames(m_animName);
}

// --- RenameAnimationCommand ---

RenameAnimationCommand::RenameAnimationCommand(SpriteDocument *doc, const QString &oldName, const QString &newName, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_oldName(oldName)
    , m_newName(newName)
{
    setText(QObject::tr("Rename Animation '%1' to '%2'").arg(oldName, newName));
}

void RenameAnimationCommand::redo()
{
    m_doc->renameAnimation(m_oldName, m_newName);
}

void RenameAnimationCommand::undo()
{
    m_doc->renameAnimation(m_newName, m_oldName);
}

// --- DuplicateAnimationCommand ---

DuplicateAnimationCommand::DuplicateAnimationCommand(SpriteDocument *doc, const QString &sourceName, const QString &newName, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_sourceName(sourceName)
    , m_newName(newName)
{
    setText(QObject::tr("Duplicate Animation '%1' as '%2'").arg(sourceName, newName));
}

void DuplicateAnimationCommand::redo()
{
    m_doc->duplicateAnimation(m_sourceName, m_newName);
}

void DuplicateAnimationCommand::undo()
{
    m_doc->removeAnimation(m_newName);
}

// --- ReorderAnimationFramesCommand ---

ReorderAnimationFramesCommand::ReorderAnimationFramesCommand(SpriteDocument *doc, const QString &animName, const QList<int> &newSequence, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_animName(animName)
    , m_newSequence(newSequence)
{
    setText(QObject::tr("Reorder Frames in Animation '%1'").arg(animName));
    if (m_doc && m_doc->hasAnimation(animName)) {
        m_oldSequence = m_doc->animation(animName).frameIndices;
    }
}

void ReorderAnimationFramesCommand::redo()
{
    m_doc->setAnimationFrameSequence(m_animName, m_newSequence);
}

void ReorderAnimationFramesCommand::undo()
{
    m_doc->setAnimationFrameSequence(m_animName, m_oldSequence);
}

// --- ChangeAnimationPropertiesCommand ---

ChangeAnimationPropertiesCommand::ChangeAnimationPropertiesCommand(SpriteDocument *doc, const QString &animName, int newFps, SpriteAnimation::LoopMode newLoopMode, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_animName(animName)
    , m_newFps(newFps)
    , m_newLoopMode(newLoopMode)
{
    setText(QObject::tr("Change Properties for Animation '%1'").arg(animName));
    if (m_doc && m_doc->hasAnimation(animName)) {
        const SpriteAnimation &anim = m_doc->animation(animName);
        m_oldFps = anim.fps;
        m_oldLoopMode = anim.loopMode;
    }
}

void ChangeAnimationPropertiesCommand::redo()
{
    if (m_doc && m_doc->hasAnimation(m_animName)) {
        SpriteAnimation anim = m_doc->animation(m_animName);
        anim.fps = m_newFps;
        anim.loopMode = m_newLoopMode;
        anim.loop = (m_newLoopMode == SpriteAnimation::Loop);
        m_doc->setAnimation(anim);
    }
}

void ChangeAnimationPropertiesCommand::undo()
{
    if (m_doc && m_doc->hasAnimation(m_animName)) {
        SpriteAnimation anim = m_doc->animation(m_animName);
        anim.fps = m_oldFps;
        anim.loopMode = m_oldLoopMode;
        anim.loop = (m_oldLoopMode == SpriteAnimation::Loop);
        m_doc->setAnimation(anim);
    }
}

// ============================================================================
// ChangeBoxRectCommand
// ============================================================================
ChangeBoxRectCommand::ChangeBoxRectCommand(SpriteDocument *doc, int boxIndex, const QRect &oldRect, const QRect &newRect, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_index(boxIndex)
    , m_oldRect(oldRect)
    , m_newRect(newRect)
{
    setText(QObject::tr("Resize/Move Slice %1").arg(boxIndex + 1));
}

void ChangeBoxRectCommand::redo()
{
    m_doc->updateBoxRect(m_index, m_newRect);
}

void ChangeBoxRectCommand::undo()
{
    m_doc->updateBoxRect(m_index, m_oldRect);
}

// ============================================================================
// AddSliceCommand
// ============================================================================
AddSliceCommand::AddSliceCommand(SpriteDocument *doc, const QRect &rect, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_rect(rect)
    , m_createdIndex(-1)
{
    setText(QObject::tr("Add Slice"));
}

void AddSliceCommand::redo()
{
    if (m_createdIndex < 0) {
        m_createdIndex = m_doc->addSlice(m_rect);
    } else {
        QImage img = m_doc->atlas().copy(m_rect);
        SpriteBox box;
        box.rect = m_rect;
        box.index = m_createdIndex;
        box.selected = true;
        m_doc->insertFrame(m_createdIndex, img, box);
    }
}

void AddSliceCommand::undo()
{
    if (m_createdIndex >= 0 && m_createdIndex < m_doc->frameCount()) {
        m_doc->removeFrame(m_createdIndex);
    }
}

// ============================================================================
// RemoveBackgroundCommand
// ============================================================================
RemoveBackgroundCommand::RemoveBackgroundCommand(SpriteDocument *doc,
                                                 const QImage &oldAtlas, const QList<QImage> &oldFrames, const QList<SpriteBox> &oldBoxes,
                                                 const QImage &newAtlas, const QList<QImage> &newFrames, const QList<SpriteBox> &newBoxes,
                                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_oldAtlas(oldAtlas)
    , m_oldFrames(oldFrames)
    , m_oldBoxes(oldBoxes)
    , m_newAtlas(newAtlas)
    , m_newFrames(newFrames)
    , m_newBoxes(newBoxes)
{
    setText(QObject::tr("Remove Background"));
    if (m_doc) {
        m_animationsBackup = m_doc->animations();
    }
}

RemoveBackgroundCommand::RemoveBackgroundCommand(SpriteDocument *doc,
                                                 const QImage &newAtlas, const QList<QImage> &newFrames, const QList<SpriteBox> &newBoxes,
                                                 QUndoCommand *parent)
    : RemoveBackgroundCommand(doc,
                              doc ? doc->atlas() : QImage(),
                              doc ? doc->frames() : QList<QImage>(),
                              doc ? doc->boxes() : QList<SpriteBox>(),
                              newAtlas, newFrames, newBoxes, parent)
{
}

void RemoveBackgroundCommand::redo()
{
    if (!m_doc) return;
    m_doc->setAtlas(m_newAtlas);
    m_doc->setFrames(m_newFrames, m_newBoxes);
}

void RemoveBackgroundCommand::undo()
{
    if (!m_doc) return;
    m_doc->setAtlas(m_oldAtlas);
    m_doc->setFrames(m_oldFrames, m_oldBoxes);
    m_doc->setAnimations(m_animationsBackup);
}

// ChangePivotCommand
ChangePivotCommand::ChangePivotCommand(SpriteDocument *doc,
                                       const QList<int> &indices,
                                       const QList<QPoint> &newPivots,
                                       bool custom,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
{
    setText(QObject::tr("KEY_CMD_CHANGE_PIVOT"));
    for (int i = 0; i < indices.size(); ++i) {
        int idx = indices[i];
        if (m_doc && idx >= 0 && idx < m_doc->boxes().size()) {
            PivotInfo info;
            info.index = idx;
            info.oldPivot = m_doc->box(idx).pivot;
            info.oldCustom = m_doc->box(idx).hasCustomPivot;
            info.newPivot = (i < newPivots.size()) ? newPivots[i] : (newPivots.isEmpty() ? QPoint(0, 0) : newPivots.first());
            info.newCustom = custom;
            m_pivots.append(info);
        }
    }
}

ChangePivotCommand::ChangePivotCommand(SpriteDocument *doc,
                                       int index,
                                       const QPoint &oldPivot,
                                       const QPoint &newPivot,
                                       bool custom,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
{
    setText(QObject::tr("KEY_CMD_CHANGE_PIVOT"));
    if (m_doc && index >= 0 && index < m_doc->boxes().size()) {
        PivotInfo info;
        info.index = index;
        info.oldPivot = oldPivot;
        info.oldCustom = m_doc->box(index).hasCustomPivot;
        info.newPivot = newPivot;
        info.newCustom = custom;
        m_pivots.append(info);
    }
}

void ChangePivotCommand::undo()
{
    if (!m_doc) return;
    for (const auto &info : m_pivots) {
        m_doc->setBoxPivot(info.index, info.oldPivot, info.oldCustom);
    }
}

void ChangePivotCommand::redo()
{
    if (!m_doc) return;
    for (const auto &info : m_pivots) {
        m_doc->setBoxPivot(info.index, info.newPivot, info.newCustom);
    }
}

// --- EditSpritePixelsCommand ---

EditSpritePixelsCommand::EditSpritePixelsCommand(SpriteDocument *doc,
                                                 int frameIndex,
                                                 const QImage &newFrame,
                                                 QUndoCommand *parent)
    : EditSpritePixelsCommand(doc, QMap<int, QImage>{{frameIndex, newFrame}}, parent)
{
}

EditSpritePixelsCommand::EditSpritePixelsCommand(SpriteDocument *doc,
                                                 const QMap<int, QImage> &modifiedFrames,
                                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_newFrames(modifiedFrames)
{
    if (m_doc) {
        for (auto it = m_newFrames.constBegin(); it != m_newFrames.constEnd(); ++it) {
            int idx = it.key();
            m_oldFrames[idx] = m_doc->frame(idx);

            if (!m_doc->atlas().isNull() && idx >= 0 && idx < m_doc->boxes().size()) {
                const SpriteBox &b = m_doc->box(idx);
                if (!b.rect.isNull()) {
                    QRect r = b.rect.intersected(m_doc->atlas().rect());
                    if (!r.isEmpty()) {
                        FramePatch fp;
                        fp.rect = r;
                        fp.oldPatch = m_doc->atlas().copy(r);
                        fp.newPatch = it.value();
                        m_framePatches.append(fp);
                    }
                }
            }
        }
    }

    if (m_newFrames.size() == 1) {
        setText(QObject::tr("Edit Frame %1 Pixels").arg(m_newFrames.firstKey() + 1));
    } else {
        setText(QObject::tr("Edit Pixels (%1 Frames)").arg(m_newFrames.size()));
    }
}

EditSpritePixelsCommand::EditSpritePixelsCommand(SpriteDocument *doc,
                                                 int frameIndex,
                                                 const QImage &oldFrame,
                                                 const QImage &newFrame,
                                                 const QImage &oldAtlas,
                                                 const QImage &newAtlas,
                                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_oldAtlas(oldAtlas)
    , m_newAtlas(newAtlas)
{
    m_oldFrames[frameIndex] = oldFrame;
    m_newFrames[frameIndex] = newFrame;
    setText(QObject::tr("Edit Frame %1 Pixels").arg(frameIndex + 1));
}

void EditSpritePixelsCommand::redo()
{
    if (!m_doc) return;
    for (const FramePatch &fp : m_framePatches) {
        m_doc->patchAtlas(fp.rect, fp.newPatch);
    }
    if (m_framePatches.isEmpty() && !m_newAtlas.isNull()) {
        m_doc->setAtlas(m_newAtlas);
    }
    for (auto it = m_newFrames.constBegin(); it != m_newFrames.constEnd(); ++it) {
        m_doc->replaceFrame(it.key(), it.value());
    }
}

void EditSpritePixelsCommand::undo()
{
    if (!m_doc) return;
    for (const FramePatch &fp : m_framePatches) {
        m_doc->patchAtlas(fp.rect, fp.oldPatch);
    }
    if (m_framePatches.isEmpty() && !m_oldAtlas.isNull()) {
        m_doc->setAtlas(m_oldAtlas);
    }
    for (auto it = m_oldFrames.constBegin(); it != m_oldFrames.constEnd(); ++it) {
        m_doc->replaceFrame(it.key(), it.value());
    }
}

