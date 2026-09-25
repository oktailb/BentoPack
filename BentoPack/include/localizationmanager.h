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

#ifndef LOCALIZATIONMANAGER_H
#define LOCALIZATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QStringList>
#include "bentopackcore_export.h"

/**
 * @brief Manages application localization dynamically at runtime.
 *
 * Handles loading, installing, and swapping QTranslator instances on QCoreApplication.
 * Installing a new translator dispatches QEvent::LanguageChange to all top-level
 * widgets in the application, allowing real-time UI language switching without
 * restarting the application or discarding project states.
 */
class SPRITESTUDIO_CORE_EXPORT LocalizationManager : public QObject
{
    Q_OBJECT

public:
    static LocalizationManager& instance();

    /**
     * @brief Initializes localization based on saved AppConfig setting.
     */
    void init();

    /**
     * @brief Dynamically switches the active UI language.
     * @param langCode Language identifier ("system", "fr_FR", "en_US", "ja_JA").
     * @return true if translation loaded and installed successfully, false otherwise.
     */
    bool setLanguage(const QString &langCode);

    /**
     * @brief Returns the currently configured language identifier.
     */
    QString currentLanguage() const { return m_currentLanguage; }

    /**
     * @brief Returns list of supported language codes ("system", "fr_FR", "en_US", "ja_JA").
     */
    static QStringList supportedLanguages();

signals:
    /**
     * @brief Emitted whenever the application language has changed.
     */
    void languageChanged(const QString &langCode);

private:
    LocalizationManager();
    ~LocalizationManager() override = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    QString m_currentLanguage = QStringLiteral("system");
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
};

#endif // LOCALIZATIONMANAGER_H
