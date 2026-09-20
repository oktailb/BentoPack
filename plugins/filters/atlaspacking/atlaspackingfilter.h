#ifndef ATLASPACKINGFILTER_H
#define ATLASPACKINGFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing in-editor MaxRects 2D bin-packing with live preview.
 */
class AtlasPackingFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit AtlasPackingFilter(QObject *parent = nullptr) : QObject(parent) {}
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
