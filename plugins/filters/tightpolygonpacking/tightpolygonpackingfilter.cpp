#include "tightpolygonpackingfilter.h"
#include "atlaspackingdialog.h"
#include <QCoreApplication>

QString TightPolygonPackingFilter::name() const
{
    return QCoreApplication::translate("TightPolygonPackingFilter", "Tight Polygon Packing (Nesting)...");
}

QString TightPolygonPackingFilter::description() const
{
    return QCoreApplication::translate("TightPolygonPackingFilter",
        "Packs sprites tightly into the atlas using tight polygonal envelopes, overlapping bounding boxes, and multi-threaded collision detection.");
}

QString TightPolygonPackingFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* TightPolygonPackingFilter::createDialog(SpriteDocument *doc,
                                                          QUndoStack *undoStack,
                                                          QWidget *parent)
{
    auto *dlg = new AtlasPackingDialog(doc, undoStack, parent);
    dlg->setWindowTitle(QCoreApplication::translate("TightPolygonPackingFilter", "Tight Polygon Packing (Nesting)"));
    dlg->setAlgorithm(5); // 5 = Tight Polygon Packing (Nesting — Overlapping Rects)
    return dlg;
}
