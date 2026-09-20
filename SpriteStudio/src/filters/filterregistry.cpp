#include "filters/filterregistry.h"
#include "widgets/filterdialogbase.h"
#include "model/spritedocument.h"
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QCoreApplication>

FilterRegistry& FilterRegistry::instance()
{
    static FilterRegistry s_instance;
    if (!s_instance.m_initialized) {
        s_instance.initDefaultFilters();
    }
    return s_instance;
}

FilterRegistry::~FilterRegistry()
{
}

void FilterRegistry::registerFilter(std::unique_ptr<FilterPlugin> filter)
{
    if (!filter) return;
    for (FilterPlugin *f : m_filters) {
        if (f == filter.get() || (!f->id().isEmpty() && f->id() == filter->id())) {
            return;
        }
    }
    m_filters.append(filter.get());
    m_ownedFilters.push_back(std::move(filter));
}

void FilterRegistry::registerFilter(FilterPlugin *filter, bool takeOwnership)
{
    if (!filter) return;
    for (FilterPlugin *f : m_filters) {
        if (f == filter || (!f->id().isEmpty() && f->id() == filter->id())) {
            return;
        }
    }
    if (takeOwnership) {
        registerFilter(std::unique_ptr<FilterPlugin>(filter));
    } else {
        m_filters.append(filter);
    }
}

FilterPlugin* FilterRegistry::findFilter(const QString &id) const
{
    for (FilterPlugin *f : m_filters) {
        if (f && f->id() == id) {
            return f;
        }
    }
    return nullptr;
}

QStringList FilterRegistry::categories() const
{
    QStringList cats;
    for (FilterPlugin *f : m_filters) {
        if (f && !cats.contains(f->category())) {
            cats.append(f->category());
        }
    }
    return cats;
}

QList<FilterPlugin*> FilterRegistry::filtersByCategory(const QString &category) const
{
    QList<FilterPlugin*> list;
    for (FilterPlugin *f : m_filters) {
        if (f && f->category() == category) {
            list.append(f);
        }
    }
    return list;
}

void FilterRegistry::initDefaultFilters()
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

void FilterRegistry::loadPlugins(const QString &dirPath)
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
                    FilterPlugin *filter = qobject_cast<FilterPlugin*>(plugin);
                    if (filter) {
                        registerFilter(filter, false);
                    }
                }
            }
        }
    }
}

void FilterRegistry::populateMenu(QMenu *menu,
                                  SpriteDocument *doc,
                                  QUndoStack *undoStack,
                                  QWidget *parentWindow)
{
    if (!menu) return;
    menu->clear();

    if (!m_initialized) {
        initDefaultFilters();
    }

    QStringList cats = categories();
    for (int i = 0; i < cats.size(); ++i) {
        const QString &cat = cats[i];
        if (i > 0) {
            menu->addSeparator();
        }

        // Section header for category
        QString catDisplay = cat;
        if (cat == QLatin1String("Cleanup"))       catDisplay = QObject::tr("Cleanup");
        else if (cat == QLatin1String("Colors"))   catDisplay = QObject::tr("Colors");
        else if (cat == QLatin1String("Effects"))  catDisplay = QObject::tr("Effects");
        else if (cat == QLatin1String("Geometry")) catDisplay = QObject::tr("Geometry");

        QAction *headerAction = menu->addSection(catDisplay);
        Q_UNUSED(headerAction);

        QList<FilterPlugin*> catFilters = filtersByCategory(cat);
        for (FilterPlugin *filter : catFilters) {
            if (!filter) continue;

            QAction *action = menu->addAction(filter->name());
            action->setToolTip(filter->description());
            if (!filter->shortcut().isEmpty()) {
                action->setShortcut(filter->shortcut());
            }
            if (!filter->icon().isNull()) {
                action->setIcon(filter->icon());
            }

            QObject *context = parentWindow ? static_cast<QObject*>(parentWindow) : static_cast<QObject*>(menu);
            connect(action, &QAction::triggered, context, [filter, doc, undoStack, parentWindow]() {
                if (!doc || doc->atlas().isNull()) {
                    QMessageBox::information(parentWindow,
                                             QObject::tr("No Atlas Loaded"),
                                             QObject::tr("Please open or import a sprite sheet first before applying a filter."));
                    return;
                }

                FilterDialogBase *dlg = filter->createDialog(doc, undoStack, parentWindow);
                if (dlg) {
                    dlg->exec();
                    delete dlg;
                }
            });
        }
    }
}
