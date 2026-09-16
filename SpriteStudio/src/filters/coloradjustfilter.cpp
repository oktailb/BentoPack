#include "filters/coloradjustfilter.h"
#include "widgets/coloradjustfilterdialog.h"
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
