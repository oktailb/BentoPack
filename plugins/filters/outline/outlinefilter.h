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

#ifndef OUTLINEFILTER_H
#define OUTLINEFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing outline and silhouette generation.
 */
class OutlineFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit OutlineFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("outline"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // OUTLINEFILTER_H
