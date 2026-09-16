#ifndef FILTERCOMMANDS_H
#define FILTERCOMMANDS_H

#include <QUndoCommand>
#include <QImage>
#include <QPixmap>
#include <QList>
#include <QMap>
#include "model/spritedocument.h"

/**
 * @brief Generic, reversible QUndoCommand for applying image and sprite filters.
 */
class ApplyFilterCommand : public QUndoCommand
{
public:
    ApplyFilterCommand(SpriteDocument *doc,
                       const QString &filterTitle,
                       const QImage &oldAtlas,
                       const QList<QPixmap> &oldFrames,
                       const QList<SpriteBox> &oldBoxes,
                       const QMap<QString, SpriteAnimation> &oldAnimations,
                       const QImage &newAtlas,
                       const QList<QPixmap> &newFrames,
                       const QList<SpriteBox> &newBoxes,
                       const QMap<QString, SpriteAnimation> &newAnimations = QMap<QString, SpriteAnimation>(),
                       QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*                 m_doc;
    QImage                          m_oldAtlas;
    QList<QPixmap>                  m_oldFrames;
    QList<SpriteBox>                m_oldBoxes;
    QMap<QString, SpriteAnimation>  m_oldAnimations;

    QImage                          m_newAtlas;
    QList<QPixmap>                  m_newFrames;
    QList<SpriteBox>                m_newBoxes;
    QMap<QString, SpriteAnimation>  m_newAnimations;
};

#endif // FILTERCOMMANDS_H
