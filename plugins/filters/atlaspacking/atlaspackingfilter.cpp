// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

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
