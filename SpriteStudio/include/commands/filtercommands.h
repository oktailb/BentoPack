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

#ifndef FILTERCOMMANDS_H
#define FILTERCOMMANDS_H

#include <QUndoCommand>
#include <QImage>
#include <QList>
#include <QMap>
#include "model/spritedocument.h"

#include "spritestudiocore_export.h"

/**
 * @brief Generic, reversible QUndoCommand for applying image and sprite filters.
 */
class SPRITESTUDIO_CORE_EXPORT ApplyFilterCommand : public QUndoCommand
{
public:
    ApplyFilterCommand(SpriteDocument *doc,
                       const QString &filterTitle,
                       const QImage &oldAtlas,
                       const QList<QImage> &oldFrames,
                       const QList<SpriteBox> &oldBoxes,
                       const QMap<QString, SpriteAnimation> &oldAnimations,
                       const QImage &newAtlas,
                       const QList<QImage> &newFrames,
                       const QList<SpriteBox> &newBoxes,
                       const QMap<QString, SpriteAnimation> &newAnimations = QMap<QString, SpriteAnimation>(),
                       QUndoCommand *parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SpriteDocument*                 m_doc;
    QImage                          m_oldAtlas;
    QList<QImage>                  m_oldFrames;
    QList<SpriteBox>                m_oldBoxes;
    QMap<QString, SpriteAnimation>  m_oldAnimations;

    QImage                          m_newAtlas;
    QList<QImage>                  m_newFrames;
    QList<SpriteBox>                m_newBoxes;
    QMap<QString, SpriteAnimation>  m_newAnimations;
};

#endif // FILTERCOMMANDS_H
