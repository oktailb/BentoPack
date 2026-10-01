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

#include "retropalettefilter.h"
#include "retropalettefilterdialog.h"
#include <QCoreApplication>

QString RetroPaletteFilter::name() const
{
    return QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering...");
}

QString RetroPaletteFilter::description() const
{
    return QCoreApplication::translate("RetroPaletteFilter",
        "Quantizes colors to authentic retro hardware palettes with optional ordered Bayer dithering.");
}

QString RetroPaletteFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Colors & Palettes");
}

FilterDialogBase* RetroPaletteFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    return new RetroPaletteFilterDialog(doc, undoStack, parent);
}

QImage RetroPaletteFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int presetIdx = params.value(QStringLiteral("preset"), 0).toInt();
    QVector<QRgb> pal = RetroPaletteFilterDialog::getPresetPalette(static_cast<RetroPaletteFilterDialog::Preset>(presetIdx));
    int ditherIdx = params.value(QStringLiteral("dither"), 0).toInt();
    int strength = params.value(QStringLiteral("strength"), 100).toInt();
    return RetroPaletteFilterDialog::applyRetroPalette(image, pal, static_cast<RetroPaletteFilterDialog::DitherMatrix>(ditherIdx), strength);
}
