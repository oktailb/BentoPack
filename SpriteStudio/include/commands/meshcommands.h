#ifndef MESHCOMMANDS_H
#define MESHCOMMANDS_H

#include <QUndoCommand>
#include <QPolygonF>
#include <QList>
#include "model/spritedocument.h"

namespace SpriteStudioCommands {

struct MeshState {
    int index = -1;
    bool hasPolygonMesh = false;
    QPolygonF polygon;
    QList<QPointF> vertices;
    QList<int> triangles;
};

/**
 * @brief Reversible command to set or clear polygon meshes for one or more sprite frames.
 */
class SetPolygonMeshCommand : public QUndoCommand
{
public:
    SetPolygonMeshCommand(SpriteDocument *doc,
                          const QList<MeshState> &newStates,
                          QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument *m_doc = nullptr;
    QList<MeshState> m_oldStates;
    QList<MeshState> m_newStates;
};

} // namespace SpriteStudioCommands

#endif // MESHCOMMANDS_H
