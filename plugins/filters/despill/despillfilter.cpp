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

#include "despillfilter.h"
#include "despillfilterdialog.h"
#include <QCoreApplication>

QString DespillFilter::name() const
{
    return QCoreApplication::translate("DespillFilter", "Despill & Edge Cleanup...");
}

QString DespillFilter::description() const
{
    return QCoreApplication::translate("DespillFilter",
        "Eliminates 1px colored fringe (green, white, magenta) along sprite borders after background extraction.");
}

QString DespillFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Cleanup & Extraction");
}

FilterDialogBase* DespillFilter::createDialog(SpriteDocument *doc,
                                              QUndoStack *undoStack,
                                              QWidget *parent)
{
    return new DespillFilterDialog(doc, undoStack, parent);
}

QImage DespillFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    QRgb fringeColor = params.value(QStringLiteral("fringeColor"), 0).toUInt();
    int tolerance = params.value(QStringLiteral("tolerance"), 20).toInt();
    QString modeStr = params.value(QStringLiteral("mode"), QStringLiteral("clamp")).toString();
    DespillFilterDialog::DespillMode mode = (modeStr == QStringLiteral("strict")) ? DespillFilterDialog::StrictAlpha : DespillFilterDialog::ColorClamping;
    return DespillFilterDialog::applyDespill(image, fringeColor, tolerance, mode);
}
