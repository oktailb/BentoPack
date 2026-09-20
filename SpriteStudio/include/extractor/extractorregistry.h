#ifndef EXTRACTORREGISTRY_H
#define EXTRACTORREGISTRY_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include "extractor/extractor.h"
#include "spritestudiocore_export.h"
#include <memory>
#include <vector>

/**
 * @brief Central registry managing built-in and dynamic Extractor plugins.
 */
class SPRITESTUDIO_CORE_EXPORT ExtractorRegistry : public QObject
{
    Q_OBJECT

public:
    static ExtractorRegistry& instance();
    ~ExtractorRegistry() override;

    void registerExtractor(std::unique_ptr<Extractor> extractor);
    void registerExtractor(Extractor *extractor, bool takeOwnership = true);
    void loadPlugins(const QString &dirPath);

    const QList<Extractor*>& extractors() const { return m_extractors; }

    Extractor* findDecoder(const QString &filePath) const;
    Extractor* findEncoder(const QString &filePathOrExt) const;
    Extractor* findEncoderByFilter(const QString &filter) const;
    Extractor* findExtractorById(const QString &id) const;

    QString openFilterString() const;
    QString saveFilterString() const;

    void initDefaultExtractors();
    void rescanPlugins();

private:
    ExtractorRegistry() = default;
    Q_DISABLE_COPY(ExtractorRegistry)

    QList<Extractor*>                       m_extractors;
    std::vector<std::unique_ptr<Extractor>> m_ownedExtractors;
    QSet<QString>                           m_loadedLibraries;
    QSet<QString>                           m_scannedDirs;
    bool                                    m_initialized = false;
};

#endif // EXTRACTORREGISTRY_H
