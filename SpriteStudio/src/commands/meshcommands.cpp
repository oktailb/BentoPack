#include "commands/meshcommands.h"

namespace SpriteStudioCommands {

SetPolygonMeshCommand::SetPolygonMeshCommand(SpriteDocument *doc,
                                             const QList<MeshState> &newStates,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_newStates(newStates)
{
    setText(QObject::tr("Set Polygon Mesh"));

    if (m_doc) {
        for (const MeshState &st : m_newStates) {
            if (st.index >= 0 && st.index < m_doc->boxes().size()) {
                const SpriteBox &b = m_doc->box(st.index);
                MeshState oldSt;
                oldSt.index = st.index;
                oldSt.hasPolygonMesh = b.hasPolygonMesh;
                oldSt.polygon = b.polygon;
                oldSt.vertices = b.vertices;
                oldSt.triangles = b.triangles;
                m_oldStates.append(oldSt);
            }
        }
    }
}

void SetPolygonMeshCommand::undo()
{
    if (!m_doc) return;

    for (const MeshState &st : m_oldStates) {
        if (st.index >= 0 && st.index < m_doc->boxes().size()) {
            SpriteBox b = m_doc->box(st.index);
            b.hasPolygonMesh = st.hasPolygonMesh;
            b.polygon = st.polygon;
            b.vertices = st.vertices;
            b.triangles = st.triangles;
            m_doc->setBox(st.index, b);
        }
    }
}

void SetPolygonMeshCommand::redo()
{
    if (!m_doc) return;

    for (const MeshState &st : m_newStates) {
        if (st.index >= 0 && st.index < m_doc->boxes().size()) {
            SpriteBox b = m_doc->box(st.index);
            b.hasPolygonMesh = st.hasPolygonMesh;
            b.polygon = st.polygon;
            b.vertices = st.vertices;
            b.triangles = st.triangles;
            m_doc->setBox(st.index, b);
        }
    }
}

} // namespace SpriteStudioCommands
