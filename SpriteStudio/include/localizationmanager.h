#ifndef LOCALIZATIONMANAGER_H
#define LOCALIZATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QStringList>

/**
 * @brief Manages application localization dynamically at runtime.
 *
 * Handles loading, installing, and swapping QTranslator instances on QCoreApplication.
 * Installing a new translator dispatches QEvent::LanguageChange to all top-level
 * widgets in the application, allowing real-time UI language switching without
 * restarting the application or discarding project states.
 */
class LocalizationManager : public QObject
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
};

#endif // LOCALIZATIONMANAGER_H
