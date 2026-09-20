#ifndef BACKGROUNDREMOVALFILTER_H
#define BACKGROUNDREMOVALFILTER_H

#include <QObject>
#include <QtPlugin>
#include "filters/filterplugin.h"

/**
 * @brief Filter plugin providing intelligent background removal and re-segmentation.
 */
class BackgroundRemovalFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit BackgroundRemovalFilter(QObject *parent = nullptr) : QObject(parent) {}
    QString id() const override { return QStringLiteral("background_removal"); }
    QString name() const override;
    QString description() const override;
    QString category() const override;
    QKeySequence shortcut() const override { return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override;
};

#endif // BACKGROUNDREMOVALFILTER_H
