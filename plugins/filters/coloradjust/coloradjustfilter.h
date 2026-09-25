// This file is part of the BentoPack Plugins.
// It is subject to the license terms in the LICENSE-PLUGINS.md file found in the plugins directory.
// Commercial use for entities exceeding $1M USD gross revenue requires a separate commercial license.

#ifndef COLORADJUSTFILTER_H
#define COLORADJUSTFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing global color adjustment (HSV and Contrast).
 */
class ColorAdjustFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit ColorAdjustFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("color_adjust"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // COLORADJUSTFILTER_H
