// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef RETROPALETTEFILTER_H
#define RETROPALETTEFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing retro palette quantization and ordered Bayer dithering.
 */
class RetroPaletteFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit RetroPaletteFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("retro_palette"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // RETROPALETTEFILTER_H
