#include "filters/colorswapfilter.h"
#include "widgets/colorswapfilterdialog.h"
#include <QCoreApplication>

QString ColorSwapFilter::name() const
{
    return QCoreApplication::translate("ColorSwapFilter", "Color Swap (Alt-Skins)...");
}

QString ColorSwapFilter::description() const
{
    return QCoreApplication::translate("ColorSwapFilter",
        "Generates character/monster variants (Player 2, elemental skins) by swapping colors while preserving shading.");
}

QString ColorSwapFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Colors & Palettes");
}

FilterDialogBase* ColorSwapFilter::createDialog(SpriteDocument *doc,
                                                QUndoStack *undoStack,
                                                QWidget *parent)
{
    return new ColorSwapFilterDialog(doc, undoStack, parent);
}
