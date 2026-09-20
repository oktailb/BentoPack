#include "atlaspackingfilter.h"
#include "atlaspackingdialog.h"
#include <QCoreApplication>

QString AtlasPackingFilter::name() const
{
    return QCoreApplication::translate("AtlasPackingFilter", "Atlas Bin-Packing (MaxRects)...");
}

QString AtlasPackingFilter::description() const
{
    return QCoreApplication::translate("AtlasPackingFilter",
        "Repacks sprites into a compact atlas using MaxRects, Power of Two, and animation-safe deduplication.");
}

QString AtlasPackingFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* AtlasPackingFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    auto *dlg = new AtlasPackingDialog(doc, undoStack, parent);
    dlg->setAlgorithm(0); // 0 = MaxRects
    return dlg;
}
