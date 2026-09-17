#include <QApplication>
#include <QTextStream>
#include <QJsonDocument>
#include "config/appconfig.h"
#include "localizationmanager.h"
#include "cli/cliparser.h"

int main(int argc, char *argv[])
{
    // Force offscreen headless operation (no display server required)
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SpriteStudio"));
    QCoreApplication::setApplicationName(QStringLiteral("SpriteStudioCli"));

    // Initialize core configuration and localization
    AppConfig::instance().load();
    LocalizationManager::instance().init();

    SpriteStudioCli::CliParser parser;
    SpriteStudioCli::CliResult result = parser.parseAndExecute(app.arguments());

    // Machine-readable JSON output
    if (parser.isJsonOutput()) {
        QJsonDocument doc(result.json);
        QTextStream(stdout) << doc.toJson(QJsonDocument::Indented);
    } else if (!parser.isQuiet() && !result.message.isEmpty()) {
        if (result.exitCode == SpriteStudioCli::ExitSuccess) {
            QTextStream(stdout) << result.message << "\n";
        } else {
            QTextStream(stderr) << "Error: " << result.message << "\n";
        }
    }

    return result.exitCode;
}
