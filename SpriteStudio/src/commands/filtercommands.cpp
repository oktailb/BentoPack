#include "commands/filtercommands.h"

ApplyFilterCommand::ApplyFilterCommand(SpriteDocument *doc,
                                       const QString &filterTitle,
                                       const QImage &oldAtlas,
                                       const QList<QImage> &oldFrames,
                                       const QList<SpriteBox> &oldBoxes,
                                       const QMap<QString, SpriteAnimation> &oldAnimations,
                                       const QImage &newAtlas,
                                       const QList<QImage> &newFrames,
                                       const QList<SpriteBox> &newBoxes,
                                       const QMap<QString, SpriteAnimation> &newAnimations,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_oldAtlas(oldAtlas)
    , m_oldFrames(oldFrames)
    , m_oldBoxes(oldBoxes)
    , m_oldAnimations(oldAnimations)
    , m_newAtlas(newAtlas)
    , m_newFrames(newFrames)
    , m_newBoxes(newBoxes)
    , m_newAnimations(newAnimations)
{
    setText(filterTitle.isEmpty() ? QStringLiteral("Apply Filter") : filterTitle);
}

void ApplyFilterCommand::redo()
{
    if (!m_doc) return;
    m_doc->setAtlas(m_newAtlas);
    m_doc->setFrames(m_newFrames, m_newBoxes);
    if (!m_newAnimations.isEmpty()) {
        m_doc->setAnimations(m_newAnimations);
    }
}

void ApplyFilterCommand::undo()
{
    if (!m_doc) return;
    m_doc->setAtlas(m_oldAtlas);
    m_doc->setFrames(m_oldFrames, m_oldBoxes);
    if (!m_oldAnimations.isEmpty()) {
        m_doc->setAnimations(m_oldAnimations);
    }
}
