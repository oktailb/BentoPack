#include "filters/despillfilter.h"
#include "widgets/despillfilterdialog.h"
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
