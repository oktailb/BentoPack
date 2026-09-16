#ifndef RETROPALETTEFILTER_H
#define RETROPALETTEFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing retro palette quantization and ordered Bayer dithering.
 */
class RetroPaletteFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("retro_palette"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // RETROPALETTEFILTER_H
