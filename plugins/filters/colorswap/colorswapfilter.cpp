#include "colorswapfilter.h"
#include "colorswapfilterdialog.h"
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

QImage ColorSwapFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    QRgb srcColor = params.value(QStringLiteral("srcColor"), 0).toUInt();
    QRgb dstColor = params.value(QStringLiteral("dstColor"), 0).toUInt();
    int tolerance = params.value(QStringLiteral("tolerance"), 15).toInt();
    bool preserveShading = params.value(QStringLiteral("preserveShading"), true).toBool();
    return ColorSwapFilterDialog::applyColorSwap(image, srcColor, dstColor, tolerance, preserveShading);
}
