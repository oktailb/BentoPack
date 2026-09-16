#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include <QImage>
#include <QList>
#include <QMap>
#include "model/spritedocument.h"

/**
 * @brief Command to delete a set of frames from the document with full undo capability.
 */
class DeleteFramesCommand : public QUndoCommand
{
public:
    DeleteFramesCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    struct FrameBackup {
        int       originalIndex;
        QImage    image;
        SpriteBox box;
    };

    SpriteDocument*                 m_doc;
    QList<int>                      m_indicesToDelete;
    QList<FrameBackup>              m_deletedFrames;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
};

/**
 * @brief Command to erase pixel data from the atlas inside selected bounding boxes and delete the frames.
 */
class EraseAtlasPixelsCommand : public QUndoCommand
{
public:
    EraseAtlasPixelsCommand(SpriteDocument *doc, const QList<int> &indices, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    struct FrameBackup {
        int       originalIndex;
        QImage    image;
        SpriteBox box;
    };

    SpriteDocument*                 m_doc;
    QList<int>                      m_indicesToDelete;
    QList<FrameBackup>              m_deletedFrames;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
    QImage                          m_atlasBefore;
    QImage                          m_atlasAfter;
};

/**
 * @brief Command to merge one frame onto another with full undo capability.
 */
class MergeFramesCommand : public QUndoCommand
{
public:
    MergeFramesCommand(SpriteDocument *doc, int sourceIndex, int targetIndex, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*                 m_doc;
    int                             m_sourceIndex;
    int                             m_targetIndex;
    QImage                          m_sourceImage;
    SpriteBox                       m_sourceBox;
    QImage                          m_targetOriginalImage;
    SpriteBox                       m_targetOriginalBox;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
};

/**
 * @brief Command to create a new animation sequence.
 */
class CreateAnimationCommand : public QUndoCommand
{
public:
    CreateAnimationCommand(SpriteDocument *doc, const QString &name, const QList<int> &frameIndices, int fps, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_name;
    QList<int>      m_frameIndices;
    int             m_fps;
};

/**
 * @brief Command to delete an animation sequence with undo support.
 */
class DeleteAnimationCommand : public QUndoCommand
{
public:
    DeleteAnimationCommand(SpriteDocument *doc, const QString &name, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_name;
    SpriteAnimation m_backup;
};

/**
 * @brief Command to reverse the order of frames in an animation.
 */
class ReverseAnimationCommand : public QUndoCommand
{
public:
    ReverseAnimationCommand(SpriteDocument *doc, const QString &animName, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_animName;
};

/**
 * @brief Command to rename an animation sequence.
 */
class RenameAnimationCommand : public QUndoCommand
{
public:
    RenameAnimationCommand(SpriteDocument *doc, const QString &oldName, const QString &newName, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_oldName;
    QString         m_newName;
};

/**
 * @brief Command to duplicate an animation sequence.
 */
class DuplicateAnimationCommand : public QUndoCommand
{
public:
    DuplicateAnimationCommand(SpriteDocument *doc, const QString &sourceName, const QString &newName, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_sourceName;
    QString         m_newName;
};

/**
 * @brief Command to reorder or update frame indices within an animation sequence.
 */
class ReorderAnimationFramesCommand : public QUndoCommand
{
public:
    ReorderAnimationFramesCommand(SpriteDocument *doc, const QString &animName, const QList<int> &newSequence, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QString         m_animName;
    QList<int>      m_oldSequence;
    QList<int>      m_newSequence;
};

/**
 * @brief Command to change animation playback properties (fps and loop mode).
 */
class ChangeAnimationPropertiesCommand : public QUndoCommand
{
public:
    ChangeAnimationPropertiesCommand(SpriteDocument *doc, const QString &animName, int newFps, SpriteAnimation::LoopMode newLoopMode, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*             m_doc;
    QString                     m_animName;
    int                         m_oldFps;
    int                         m_newFps;
    SpriteAnimation::LoopMode   m_oldLoopMode;
    SpriteAnimation::LoopMode   m_newLoopMode;
};

/**
 * @brief Command to change a bounding box rectangle (resize or move) with undo/redo.
 */
class ChangeBoxRectCommand : public QUndoCommand
{
public:
    ChangeBoxRectCommand(SpriteDocument *doc, int boxIndex, const QRect &oldRect, const QRect &newRect, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    int             m_index;
    QRect           m_oldRect;
    QRect           m_newRect;
};

/**
 * @brief Command to manually add a new slice with undo/redo.
 */
class AddSliceCommand : public QUndoCommand
{
public:
    AddSliceCommand(SpriteDocument *doc, const QRect &rect, QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument* m_doc;
    QRect           m_rect;
    int             m_createdIndex = -1;
};

/**
 * @brief Command to apply background removal on the atlas and update extracted frames with undo/redo.
 */
class RemoveBackgroundCommand : public QUndoCommand
{
public:
    RemoveBackgroundCommand(SpriteDocument *doc,
                            const QImage &oldAtlas, const QList<QImage> &oldFrames, const QList<SpriteBox> &oldBoxes,
                            const QImage &newAtlas, const QList<QImage> &newFrames, const QList<SpriteBox> &newBoxes,
                            QUndoCommand *parent = nullptr);

    RemoveBackgroundCommand(SpriteDocument *doc,
                            const QImage &newAtlas, const QList<QImage> &newFrames, const QList<SpriteBox> &newBoxes,
                            QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*                 m_doc;
    QImage                          m_oldAtlas;
    QList<QImage>                  m_oldFrames;
    QList<SpriteBox>                m_oldBoxes;
    QImage                          m_newAtlas;
    QList<QImage>                  m_newFrames;
    QList<SpriteBox>                m_newBoxes;
    QMap<QString, SpriteAnimation>  m_animationsBackup;
};

/**
 * @brief Command to change pivot point(s) of one or multiple boxes with undo/redo.
 */
class ChangePivotCommand : public QUndoCommand
{
public:
    struct PivotInfo {
        int index;
        QPoint oldPivot;
        bool oldCustom;
        QPoint newPivot;
        bool newCustom;
    };

    ChangePivotCommand(SpriteDocument *doc,
                       const QList<int> &indices,
                       const QList<QPoint> &newPivots,
                       bool custom = true,
                       QUndoCommand *parent = nullptr);

    ChangePivotCommand(SpriteDocument *doc,
                       int index,
                       const QPoint &oldPivot,
                       const QPoint &newPivot,
                       bool custom = true,
                       QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*    m_doc;
    QList<PivotInfo>   m_pivots;
};

#endif // COMMANDS_H
