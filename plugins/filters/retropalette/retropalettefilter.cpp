#include "retropalettefilter.h"
#include "retropalettefilterdialog.h"
#include <QCoreApplication>

QString RetroPaletteFilter::name() const
{
    return QCoreApplication::translate("RetroPaletteFilter", "Retro Palette & Dithering...");
}

QString RetroPaletteFilter::description() const
{
    return QCoreApplication::translate("RetroPaletteFilter",
        "Quantizes colors to authentic retro hardware palettes with optional ordered Bayer dithering.");
}

QString RetroPaletteFilter::category() const
{
    return QCoreApplication::translate("FilterRegistry", "Colors & Palettes");
}

FilterDialogBase* RetroPaletteFilter::createDialog(SpriteDocument *doc,
                                                   QUndoStack *undoStack,
                                                   QWidget *parent)
{
    return new RetroPaletteFilterDialog(doc, undoStack, parent);
}

QImage RetroPaletteFilter::applyImage(const QImage &image, const QVariantMap &params)
{
    int presetIdx = params.value(QStringLiteral("preset"), 0).toInt();
    QVector<QRgb> pal = RetroPaletteFilterDialog::getPresetPalette(static_cast<RetroPaletteFilterDialog::Preset>(presetIdx));
    int ditherIdx = params.value(QStringLiteral("dither"), 0).toInt();
    int strength = params.value(QStringLiteral("strength"), 100).toInt();
    return RetroPaletteFilterDialog::applyRetroPalette(image, pal, static_cast<RetroPaletteFilterDialog::DitherMatrix>(ditherIdx), strength);
}
