/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BACKGROUNDREMOVALFILTER_H
#define BACKGROUNDREMOVALFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing intelligent background removal and re-segmentation.
 */
class BackgroundRemovalFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit BackgroundRemovalFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("background_removal"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;
    QKeySequence shortcut() const override { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // BACKGROUNDREMOVALFILTER_H
