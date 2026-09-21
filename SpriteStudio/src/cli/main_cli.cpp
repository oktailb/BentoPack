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
