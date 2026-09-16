#ifndef OUTLINEFILTER_H
#define OUTLINEFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing outline and silhouette generation.
 */
class OutlineFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("outline"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // OUTLINEFILTER_H
