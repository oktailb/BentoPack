#ifndef SAMPLE_EXTRACTOR_H
#define SAMPLE_EXTRACTOR_H

#include <extractor/extractor.h>
#include <QtPlugin>

class SampleExtractor : public Extractor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Extractor_iid)
    Q_INTERFACES(Extractor)

public:
    explicit SampleExtractor(QObject *parent = nullptr) : Extractor(parent) {}

    QString id() const override { return QStringLiteral("sample_custom_extractor"); }
    QString displayName() const override { return QStringLiteral("Sample Custom Extractor"); }
    QString description() const override { return QStringLiteral("Third-party sample plugin built using spritestudio-dev SDK."); }
    QStringList supportedExtensions() const override { return { QStringLiteral("sample") }; }
    Capabilities capabilities() const override { return CanImport | CanExport; }

    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;
};

#endif // SAMPLE_EXTRACTOR_H
