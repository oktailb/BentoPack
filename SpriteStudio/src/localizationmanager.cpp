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

#include "include/localizationmanager.h"
#include "include/config/appconfig.h"
#include <QCoreApplication>
#include <QLocale>
#include <QLibraryInfo>
#include <QDir>
#include <QDebug>

LocalizationManager& LocalizationManager::instance()
{
    static LocalizationManager inst;
    return inst;
}

LocalizationManager::LocalizationManager()
    : QObject(nullptr)
{
}

QStringList LocalizationManager::supportedLanguages()
{
    return {
        QStringLiteral("system"),
        QStringLiteral("fr_FR"),
        QStringLiteral("en_US"),
        QStringLiteral("ja_JA")
    };
}

void LocalizationManager::init()
{
    const QString savedLang = AppConfig::instance().general().language;
    setLanguage(savedLang.isEmpty() ? QStringLiteral("system") : savedLang);
}

bool LocalizationManager::setLanguage(const QString &langCode)
{
    m_currentLanguage = langCode.isEmpty() ? QStringLiteral("system") : langCode;

    QString locale;
    if (m_currentLanguage == QStringLiteral("system")) {
        locale = QLocale::system().name();
    } else {
        locale = m_currentLanguage;
    }

    QStringList candidates;
    candidates << ("sprite_studio_" + locale);
    if (locale.startsWith(QStringLiteral("fr"), Qt::CaseInsensitive)) {
        candidates << QStringLiteral("sprite_studio_fr_FR");
    } else if (locale.startsWith(QStringLiteral("ja"), Qt::CaseInsensitive)) {
        candidates << QStringLiteral("sprite_studio_ja_JA");
    } else if (locale.startsWith(QStringLiteral("en"), Qt::CaseInsensitive)) {
        candidates << QStringLiteral("sprite_studio_en_US");
    }
    candidates << QStringLiteral("sprite_studio_en_US"); // ultimate fallback

    // Remove existing translators before loading the new catalogs
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);

    // Try loading Qt standard base translations (qtbase_fr.qm, qtbase_ja.qm, etc.)
    QString qtBaseLocale = locale.startsWith(QStringLiteral("fr"), Qt::CaseInsensitive) ? QStringLiteral("fr")
                         : (locale.startsWith(QStringLiteral("ja"), Qt::CaseInsensitive) ? QStringLiteral("ja")
                         : QStringLiteral("en"));
    if (m_qtTranslator.load(QStringLiteral("qtbase_") + qtBaseLocale, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
        m_qtTranslator.load(QStringLiteral("qt_") + qtBaseLocale, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&m_qtTranslator);
    }

    bool loaded = false;
    for (const QString &cand : candidates) {
        if (m_appTranslator.load(cand, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
            m_appTranslator.load(cand, QCoreApplication::applicationDirPath() + QStringLiteral("/i18n")) ||
            m_appTranslator.load(cand, QStringLiteral(":/i18n/"))) {
            loaded = true;
            break;
        }
    }

    if (loaded) {
        // Installing the translator automatically posts QEvent::LanguageChange
        // to all top-level widgets in the Qt application.
        QCoreApplication::installTranslator(&m_appTranslator);
    }

    emit languageChanged(m_currentLanguage);
    return loaded;
}
