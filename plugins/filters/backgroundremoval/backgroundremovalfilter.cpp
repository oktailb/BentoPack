// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "backgroundremovalfilter.h"
#include "backgroundremovaldialog.h"
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
