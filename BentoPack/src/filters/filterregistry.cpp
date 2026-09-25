/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#include "filters/filterregistry.h"
#include "model/spritedocument.h"
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

    // 1. Environment variable BENTOPACK_PLUGIN_PATH
    const QString envPath = QString::fromUtf8(qgetenv("BENTOPACK_PLUGIN_PATH"));
    if (!envPath.isEmpty()) {
        searchDirs << envPath.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    }

    // 2. Application directory plugins
    QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << QDir(appDir).filePath(QStringLiteral("plugins"));
    searchDirs << QDir(appDir).filePath(QStringLiteral("../bin/plugins"));

    // 3. System installed library paths (e.g. /usr/lib/bentopack/plugins)
    searchDirs << QDir(appDir).filePath(QStringLiteral("../lib/bentopack/plugins"));
    searchDirs << QStringLiteral("/usr/lib/bentopack/plugins");
    searchDirs << QStringLiteral("/usr/local/lib/bentopack/plugins");

    for (const QString &dir : searchDirs) {
        loadPlugins(dir);
    }
}

void FilterRegistry::rescanPlugins()
{
    m_scannedDirs.clear();

    QStringList searchDirs;
    const QString envPath = QString::fromUtf8(qgetenv("BENTOPACK_PLUGIN_PATH"));
    if (!envPath.isEmpty()) {
        searchDirs << envPath.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    }
    QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << QDir(appDir).filePath(QStringLiteral("plugins"));
    searchDirs << QDir(appDir).filePath(QStringLiteral("../bin/plugins"));
    searchDirs << QDir(appDir).filePath(QStringLiteral("../lib/bentopack/plugins"));
    searchDirs << QStringLiteral("/usr/lib/bentopack/plugins");
    searchDirs << QStringLiteral("/usr/local/lib/bentopack/plugins");

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

