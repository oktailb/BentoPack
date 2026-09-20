#ifndef COLORSWAPFILTER_H
#define COLORSWAPFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing color swapping and alt-skin palette generation.
 */
class ColorSwapFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit ColorSwapFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("color_swap"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // COLORSWAPFILTER_H
