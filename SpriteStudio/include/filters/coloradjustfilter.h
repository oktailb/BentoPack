#ifndef COLORADJUSTFILTER_H
#define COLORADJUSTFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing global color adjustment (HSV and Contrast).
 */
class ColorAdjustFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("color_adjust"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // COLORADJUSTFILTER_H
