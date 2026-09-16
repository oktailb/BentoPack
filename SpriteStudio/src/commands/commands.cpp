#include "commands/commands.h"
#include <algorithm>

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
            m_deletedFrames.append(fb);
        }
    }

    m_animationsBackup = m_doc->animations();
    m_atlasBefore = m_doc->atlas();
    m_atlasAfter = (m_atlasBefore.format() == QImage::Format_ARGB32)
        ? m_atlasBefore.copy()
        : m_atlasBefore.convertToFormat(QImage::Format_ARGB32);

    for (const FrameBackup &fb : m_deletedFrames) {
        QRect r = fb.box.rect.intersected(m_atlasAfter.rect());
        for (int y = r.top(); y <= r.bottom(); ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(m_atlasAfter.scanLine(y));
            for (int x = r.left(); x <= r.right(); ++x) {
                line[x] = qRgba(0, 0, 0, 0);
            }
        }
    }
}

void EraseAtlasPixelsCommand::redo()
{
    m_doc->setAtlas(m_atlasAfter);
    m_doc->removeFrames(m_indicesToDelete);
}

void EraseAtlasPixelsCommand::undo()
{
    m_doc->setAtlas(m_atlasBefore);
    for (const FrameBackup &fb : m_deletedFrames) {
        m_doc->insertFrame(fb.originalIndex, fb.image, fb.box);
    }
    for (auto it = m_animationsBackup.begin(); it != m_animationsBackup.end(); ++it) {
        m_doc->setAnimation(it.key(), it.value().frameIndices, it.value().fps, it.value().loop);
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

