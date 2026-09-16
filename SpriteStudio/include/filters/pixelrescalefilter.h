#ifndef PIXELRESCALEFILTER_H
#define PIXELRESCALEFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing pixel art rescaling (Nearest-Neighbor & Scale2x).
 */
class PixelRescaleFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("pixel_rescale"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // PIXELRESCALEFILTER_H
