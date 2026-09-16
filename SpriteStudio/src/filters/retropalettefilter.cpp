#include "filters/retropalettefilter.h"
#include "widgets/retropalettefilterdialog.h"
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
