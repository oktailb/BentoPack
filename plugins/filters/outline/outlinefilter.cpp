#include "outlinefilter.h"
#include "outlinefilterdialog.h"
#include <QCoreApplication>

QString OutlineFilter::name() const
{
    return QCoreApplication::translate("OutlineFilter", "Outline & Silhouette Generator...");
}

QString OutlineFilter::description() const
{
    return QCoreApplication::translate("OutlineFilter",
        "Adds a distinct outline (1-4 px) around sprites (sticker effect, visibility) or creates solid silhouettes (hit-flash).");
}

QString OutlineFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Effects & Outlines");
}

FilterDialogBase* OutlineFilter::createDialog(SpriteDocument *doc,
                                              QUndoStack *undoStack,
                                              QWidget *parent)
{
    return new OutlineFilterDialog(doc, undoStack, parent);
}

QImage OutlineFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int thickness = params.value(QStringLiteral("thickness"), 1).toInt();
    QRgb color = params.value(QStringLiteral("color"), qRgb(0, 0, 0)).toUInt();
    bool silhouette = params.value(QStringLiteral("silhouette"), false).toBool();
    return OutlineFilterDialog::applyOutline(image, thickness, color, OutlineFilterDialog::EightConnected, silhouette);
}
