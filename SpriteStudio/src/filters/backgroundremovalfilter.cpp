#include "filters/backgroundremovalfilter.h"
#include "widgets/backgroundremovaldialog.h"
#include <QCoreApplication>

QString BackgroundRemovalFilter::name() const
{
    return QCoreApplication::translate("BackgroundRemovalFilter", "Background Removal...");
}

QString BackgroundRemovalFilter::description() const
{
    return QCoreApplication::translate("BackgroundRemovalFilter",
        "Detects dominant background color and makes pixels transparent with automatic bounding box recalculation.");
}

QString BackgroundRemovalFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Cleanup & Extraction");
}

FilterDialogBase* BackgroundRemovalFilter::createDialog(SpriteDocument *doc,
                                                        QUndoStack *undoStack,
                                                        QWidget *parent)
{
    return new BackgroundRemovalDialog(doc, undoStack, parent);
}
