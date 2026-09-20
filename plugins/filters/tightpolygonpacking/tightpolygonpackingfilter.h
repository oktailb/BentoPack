// This file is part of the SpriteStudio Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef TIGHTPOLYGONPACKINGFILTER_H
#define TIGHTPOLYGONPACKINGFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing in-editor Tight Polygon Packing with real-time nesting preview.
 */
class TightPolygonPackingFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit TightPolygonPackingFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("tight_polygon_packing"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;
    QKeySequence shortcut() const override { return QKeySequence(QStringLiteral("Ctrl+Shift+T")); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // TIGHTPOLYGONPACKINGFILTER_H
