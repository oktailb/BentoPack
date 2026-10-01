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

#include "coloradjustfilter.h"
#include "coloradjustfilterdialog.h"
#include <QCoreApplication>

QString ColorAdjustFilter::name() const
{
    return QCoreApplication::translate("ColorAdjustFilter", "Color Adjustment (HSV & Contrast)...");
}

QString ColorAdjustFilter::description() const
{
    return QCoreApplication::translate("ColorAdjustFilter",
        "Adjusts hue rotation, saturation, brightness, and contrast globally or on selected frames.");
}

QString ColorAdjustFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Colors & Palettes");
}

FilterDialogBase* ColorAdjustFilter::createDialog(SpriteDocument *doc,
                                                  QUndoStack *undoStack,
                                                  QWidget *parent)
{
    return new ColorAdjustFilterDialog(doc, undoStack, parent);
}

QImage ColorAdjustFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int hue = params.value(QStringLiteral("hue"), 0).toInt();
    int saturation = params.value(QStringLiteral("saturation"), 0).toInt();
    int value = params.value(QStringLiteral("value"), 0).toInt();
    int contrast = params.value(QStringLiteral("contrast"), 0).toInt();
    return ColorAdjustFilterDialog::applyColorAdjust(image, hue, saturation, value, contrast);
}
