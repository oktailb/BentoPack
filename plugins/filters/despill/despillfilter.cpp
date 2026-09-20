#include "despillfilter.h"
#include "despillfilterdialog.h"
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

QImage DespillFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    QRgb fringeColor = params.value(QStringLiteral("fringeColor"), 0).toUInt();
    int tolerance = params.value(QStringLiteral("tolerance"), 20).toInt();
    QString modeStr = params.value(QStringLiteral("mode"), QStringLiteral("clamp")).toString();
    DespillFilterDialog::DespillMode mode = (modeStr == QStringLiteral("strict")) ? DespillFilterDialog::StrictAlpha : DespillFilterDialog::ColorClamping;
    return DespillFilterDialog::applyDespill(image, fringeColor, tolerance, mode);
}
