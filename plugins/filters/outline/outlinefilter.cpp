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

#include "outlinefilter.h"
#include "outlinefilterdialog.h"
#include <QCoreApplication>

QString OutlineFilter::name() const
{
    return QCoreApplication::translate("OutlineFilter", "Outline & Silhouette Generator...");
}

QString OutlineFilter::description() const
{
    return QCoreApplication::translate("OutlineFilter",
        "Adds a distinct outline (1-4 px) around sprites (sticker effect, visibility) or creates solid silhouettes (hit-flash).");
}

QString OutlineFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Effects & Outlines");
}

FilterDialogBase* OutlineFilter::createDialog(SpriteDocument *doc,
                                              QUndoStack *undoStack,
                                              QWidget *parent)
{
    return new OutlineFilterDialog(doc, undoStack, parent);
}

QImage OutlineFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int thickness = params.value(QStringLiteral("thickness"), 1).toInt();
    QRgb color = params.value(QStringLiteral("color"), qRgb(0, 0, 0)).toUInt();
    bool silhouette = params.value(QStringLiteral("silhouette"), false).toBool();
    return OutlineFilterDialog::applyOutline(image, thickness, color, OutlineFilterDialog::EightConnected, silhouette);
}
