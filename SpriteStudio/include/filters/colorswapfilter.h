#ifndef COLORSWAPFILTER_H
#define COLORSWAPFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing color swapping and alt-skin palette generation.
 */
class ColorSwapFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("color_swap"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // COLORSWAPFILTER_H
