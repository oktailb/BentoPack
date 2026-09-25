#ifndef SAMPLE_FILTER_H
#define SAMPLE_FILTER_H

#include <filters/filterplugin.h>
#include <QObject>
#include <QtPlugin>

class SampleFilter : public QObject, public FilterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FilterPlugin_iid)
    Q_INTERFACES(FilterPlugin)

public:
    explicit SampleFilter(QObject *parent = nullptr) : QObject(parent) {}

    QString id() const override { return QStringLiteral("sample_invert_filter"); }
    QString name() const override { return QStringLiteral("Sample Invert Color"); }
    QString description() const override { return QStringLiteral("Reference filter plugin inverting RGB pixel channels."); }
    QString category() const override { return QStringLiteral("Color"); }

    FilterDialogBase* createDialog(SpriteDocument *doc,
                                   QUndoStack *undoStack = nullptr,
                                   QWidget *parent = nullptr) override
    {
        Q_UNUSED(doc);
        Q_UNUSED(undoStack);
        Q_UNUSED(parent);
        return nullptr;
    }

    QImage applyImage(const QImage &image, const QVariantMap &params = QVariantMap()) override;
};

#endif // SAMPLE_FILTER_H
