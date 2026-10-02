/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "include/mainwindow.h"
#include "include/config/appconfig.h"
#include "include/localizationmanager.h"
#include "generated/version.h"
#include <QApplication>
#include <QCommandLineParser>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(QStringLiteral(":/drawer/icons/bentopack.png")));
  QCoreApplication::setOrganizationName(QStringLiteral("BentoPack"));
  QCoreApplication::setApplicationName(QStringLiteral("BentoPack"));
  QCoreApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));

  // Load configuration and initialize localization dynamically
  AppConfig::instance().load();
  LocalizationManager::instance().init();

#if defined(Q_OS_WIN)
  // Attach to parent console if running from terminal with help or version flags
  for (int i = 1; i < argc; ++i) {
      const QString arg = QString::fromLocal8Bit(argv[i]);
      if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v") ||
          arg == QStringLiteral("--help") || arg == QStringLiteral("-h") ||
          arg == QStringLiteral("-?")) {
          if (AttachConsole(ATTACH_PARENT_PROCESS)) {
              (void)freopen("CONOUT$", "w", stdout);
              (void)freopen("CONOUT$", "w", stderr);
          }
          break;
      }
  }
#endif

  QCommandLineParser parser;
  parser.setApplicationDescription(QCoreApplication::translate("main", "BentoPack - 2D Sprite Sheet Packer & Game Engine Asset Pipeline"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main", "Project file (.bento) to open"), QStringLiteral("[file]"));

  parser.process(a);

  MainWindow w;

  const QStringList positionalArgs = parser.positionalArguments();
  if (!positionalArgs.isEmpty()) {
      w.processFile(positionalArgs.first());
  }

  w.show();

  return a.exec();
}

