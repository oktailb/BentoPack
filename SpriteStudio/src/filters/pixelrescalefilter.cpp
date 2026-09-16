#include "filters/pixelrescalefilter.h"
#include "widgets/pixelrescalefilterdialog.h"
#include <QCoreApplication>

QString PixelRescaleFilter::name() const
{
    return QCoreApplication::translate("PixelRescaleFilter", "Pixel Art Rescale...");
}

QString PixelRescaleFilter::description() const
{
    return QCoreApplication::translate("PixelRescaleFilter",
        "Rescales the atlas cleanly using Nearest-Neighbor (pixel-perfect) or Scale2x (smooth contours).");
}

QString PixelRescaleFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Geometry & Transform");
}

FilterDialogBase* PixelRescaleFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    return new PixelRescaleFilterDialog(doc, undoStack, parent);
}
