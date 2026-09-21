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
    if (!m_newAnimations.isEmpty() || !m_oldAnimations.isEmpty()) {
        m_doc->setAnimations(m_newAnimations);
    }
}

void ApplyFilterCommand::undo()
{
    if (!m_doc) return;
    m_doc->setAtlas(m_oldAtlas);
    m_doc->setFrames(m_oldFrames, m_oldBoxes);
    if (!m_newAnimations.isEmpty() || !m_oldAnimations.isEmpty()) {
        m_doc->setAnimations(m_oldAnimations);
    }
}
