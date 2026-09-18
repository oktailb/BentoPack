#ifndef UNITYEXTRACTOR_H
#define UNITYEXTRACTOR_H

#include "extractor/extractor.h"

/**
 * @brief Exporter for Unity 2D Sprite Mesh Mode Tight.
 *
 * Produces a companion PNG atlas and a JSON descriptor with vertices, UVs,
 * and triangle index buffers directly compatible with Unity's Sprite.OverrideGeometry.
 */
class UnityExtractor : public Extractor
{
    Q_OBJECT

public:
    explicit UnityExtractor(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("unity"); }
    QString displayName() const override { return tr("Unity 2D Sprite Mesh (*.unity.json)"); }
    QString description() const override { return tr("Exports Unity 2D Sprite Mesh with vertex & triangle buffers."); }
    QStringList supportedExtensions() const override { return { QStringLiteral("unity.json"), QStringLiteral("json") }; }
    Capabilities capabilities() const override { return CanExport | SupportsAnimations | SupportsAtlasMetadata; }

    bool canDecode(const QString &filePath) const override;
    bool read(const QString &filePath, SpriteDocument &outDoc, ExtractorError *error = nullptr) override;
    bool write(const QString &filePath, const SpriteDocument &inDoc, const ExportOptions &options, ExtractorError *error = nullptr) override;
};

#endif // UNITYEXTRACTOR_H
