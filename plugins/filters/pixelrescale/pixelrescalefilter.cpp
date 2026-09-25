// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#include "pixelrescalefilter.h"
#include "pixelrescalefilterdialog.h"
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

QImage PixelRescaleFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    double factor = params.value(QStringLiteral("factor"), 2.0).toDouble();
    QString algoStr = params.value(QStringLiteral("algorithm"), QStringLiteral("scale2x")).toString().toLower();
    PixelRescaleFilterDialog::Algorithm algo = PixelRescaleFilterDialog::Scale2x;
    if (algoStr == QStringLiteral("nearest") || algoStr == QStringLiteral("nearestneighbor")) {
        algo = PixelRescaleFilterDialog::NearestNeighbor;
    }
    return PixelRescaleFilterDialog::applyRescale(image, factor, algo);
}
