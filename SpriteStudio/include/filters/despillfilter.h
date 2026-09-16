#ifndef DESPILLFILTER_H
#define DESPILLFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing edge cleanup and anti-halo (despill) processing.
 */
class DespillFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("despill"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // DESPILLFILTER_H
