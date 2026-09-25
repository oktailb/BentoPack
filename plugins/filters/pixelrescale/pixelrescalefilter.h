// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef PIXELRESCALEFILTER_H
#define PIXELRESCALEFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing pixel art rescaling (Nearest-Neighbor & Scale2x).
 */
class PixelRescaleFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit PixelRescaleFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("pixel_rescale"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // PIXELRESCALEFILTER_H
