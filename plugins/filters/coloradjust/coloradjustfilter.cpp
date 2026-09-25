// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

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
