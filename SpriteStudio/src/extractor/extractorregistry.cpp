#include "extractor/extractorregistry.h"
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QCoreApplication>

ExtractorRegistry& ExtractorRegistry::instance()
{
    static ExtractorRegistry reg;
    if (!reg.m_initialized) {
        reg.initDefaultExtractors();
    }
    return reg;
}

ExtractorRegistry::~ExtractorRegistry()
{
    m_extractors.clear();
    m_ownedExtractors.clear();
}

void ExtractorRegistry::registerExtractor(std::unique_ptr<Extractor> extractor)
{
    if (!extractor) return;
    Extractor *raw = extractor.get();
    for (Extractor *e : m_extractors) {
        if (e == raw || (!e->id().isEmpty() && e->id() == raw->id())) {
            return;
        }
    }

    m_extractors.append(raw);
    m_ownedExtractors.push_back(std::move(extractor));
}

void ExtractorRegistry::registerExtractor(Extractor *extractor, bool takeOwnership)
{
    if (!extractor) return;
    for (Extractor *e : m_extractors) {
        if (e == extractor || (!e->id().isEmpty() && e->id() == extractor->id())) {
            return;
        }
    }

    if (takeOwnership) {
        registerExtractor(std::unique_ptr<Extractor>(extractor));
    } else {
        m_extractors.append(extractor);
    }
}

void ExtractorRegistry::initDefaultExtractors()
{
    if (m_initialized) return;
    m_initialized = true;

    QStringList searchDirs;

    // 1. Environment variable SPRITESTUDIO_PLUGIN_PATH
    const QString envPath = QString::fromUtf8(qgetenv("SPRITESTUDIO_PLUGIN_PATH"));
    if (!envPath.isEmpty()) {
        searchDirs << envPath.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    }

    // 2. Application directory plugins
    QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << QDir(appDir).filePath(QStringLiteral("plugins"));
    searchDirs << QDir(appDir).filePath(QStringLiteral("../bin/plugins"));

    // 3. System installed library paths (e.g. /usr/lib/spritestudio/plugins)
    searchDirs << QDir(appDir).filePath(QStringLiteral("../lib/spritestudio/plugins"));
    searchDirs << QStringLiteral("/usr/lib/spritestudio/plugins");
    searchDirs << QStringLiteral("/usr/local/lib/spritestudio/plugins");

    for (const QString &dir : searchDirs) {
        loadPlugins(dir);
    }
}

void ExtractorRegistry::loadPlugins(const QString &dirPath)
{
    QDir pluginsDir(dirPath);
    if (!pluginsDir.exists()) return;

    const QString canonicalDir = pluginsDir.canonicalPath();
    if (canonicalDir.isEmpty() || m_scannedDirs.contains(canonicalDir)) {
        return;
    }
    m_scannedDirs.insert(canonicalDir);

    const auto entries = pluginsDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            loadPlugins(info.absoluteFilePath());
        } else if (info.isFile()) {
            const QString ext = info.suffix().toLower();
            if (ext == QStringLiteral("so") || ext == QStringLiteral("dll") || ext == QStringLiteral("dylib")) {
                const QString canonicalFile = info.canonicalFilePath();
                if (canonicalFile.isEmpty() || m_loadedLibraries.contains(canonicalFile)) {
                    continue;
                }
                m_loadedLibraries.insert(canonicalFile);

                QPluginLoader loader(canonicalFile);
                QObject *plugin = loader.instance();
                if (plugin) {
                    Extractor *extractor = qobject_cast<Extractor*>(plugin);
                    if (extractor) {
                        registerExtractor(extractor, false);
                    }
                }
            }
        }
    }
}

Extractor* ExtractorRegistry::findDecoder(const QString &filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanImport) &&
            extractor->supportedExtensions().contains(ext) &&
            extractor->canDecode(filePath)) {
            return extractor;
        }
    }

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanImport) && extractor->canDecode(filePath)) {
            return extractor;
        }
    }

    return nullptr;
}

Extractor* ExtractorRegistry::findEncoder(const QString &filePathOrExt) const
{
    QString ext = filePathOrExt.contains('.') ? QFileInfo(filePathOrExt).suffix().toLower() : filePathOrExt.toLower();

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanExport) && extractor->supportedExtensions().contains(ext)) {
            return extractor;
        }
    }
    return nullptr;
}

Extractor* ExtractorRegistry::findEncoderByFilter(const QString &filter) const
{
    if (filter.isEmpty()) return nullptr;

    for (Extractor *extractor : m_extractors) {
        if ((extractor->capabilities() & Extractor::CanExport) &&
            filter.contains(extractor->displayName(), Qt::CaseInsensitive)) {
            return extractor;
        }
    }
    return nullptr;
}

Extractor* ExtractorRegistry::findExtractorById(const QString &id) const
{
    for (Extractor *extractor : m_extractors) {
        if (extractor->id() == id) {
            return extractor;
        }
    }
    return nullptr;
}

QString ExtractorRegistry::openFilterString() const
{
    QString allExtensions;
    QStringList individualFilters;

    for (Extractor *extractor : m_extractors) {
        if (!(extractor->capabilities() & Extractor::CanImport)) continue;

        QStringList wildcards;
        for (const QString &ext : extractor->supportedExtensions()) {
            wildcards.append("*." + ext);
            if (!allExtensions.contains("*." + ext)) {
                if (!allExtensions.isEmpty()) allExtensions += " ";
                allExtensions += "*." + ext;
            }
        }
        QString name = extractor->displayName();
        if (!name.contains('(')) {
            name = QString("%1 (%2)").arg(name, wildcards.join(" "));
        }
        individualFilters.append(name);
    }

    QString result;
    if (!allExtensions.isEmpty()) {
        result = QString("All Supported Files (%1);;").arg(allExtensions);
    }
    result += individualFilters.join(";;");
    return result;
}

QString ExtractorRegistry::saveFilterString() const
{
    QStringList individualFilters;

    for (Extractor *extractor : m_extractors) {
        if (!(extractor->capabilities() & Extractor::CanExport)) continue;

        QStringList wildcards;
        for (const QString &ext : extractor->supportedExtensions()) {
            wildcards.append("*." + ext);
        }
        QString name = extractor->displayName();
        if (!name.contains('(')) {
            name = QString("%1 (%2)").arg(name, wildcards.join(" "));
        }
        individualFilters.append(name);
    }

    return individualFilters.join(";;");
}
