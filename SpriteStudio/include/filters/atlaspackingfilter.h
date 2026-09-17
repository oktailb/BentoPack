#ifndef ATLASPACKINGFILTER_H
#define ATLASPACKINGFILTER_H

#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing in-editor MaxRects 2D bin-packing with live preview.
 */
class AtlasPackingFilter : public FilterPlugin
{
public:
    QString id() const override { return QStringLiteral("atlas_packing"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;
    QKeySequence shortcut() const override { return QKeySequence(QStringLiteral("Ctrl+Shift+P")); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // ATLASPACKINGFILTER_H
