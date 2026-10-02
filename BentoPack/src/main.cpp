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
#include <iostream>

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

  // Handle command-line flags directly to avoid GUI modal message box popups on Windows
  for (int i = 1; i < argc; ++i) {
      const QString arg = QString::fromLocal8Bit(argv[i]);
      if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
#if defined(Q_OS_WIN)
          if (AttachConsole(ATTACH_PARENT_PROCESS)) {
              (void)freopen("CONOUT$", "w", stdout);
          }
#endif
          std::cout << "BentoPack " << PROJECT_VERSION << std::endl;
          return 0;
      }
      if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h") || arg == QStringLiteral("-?")) {
#if defined(Q_OS_WIN)
          if (AttachConsole(ATTACH_PARENT_PROCESS)) {
              (void)freopen("CONOUT$", "w", stdout);
          }
#endif
          const QString help = QCoreApplication::translate("main",
              "Usage: bentopack [options] [file.bento]\n\n"
              "BentoPack - 2D Sprite Sheet Packer & Game Engine Asset Pipeline\n\n"
              "Options:\n"
              "  -h, --help     Displays help on commandline options.\n"
              "  -v, --version  Displays version information.\n\n"
              "Arguments:\n"
              "  file           Project file (.bento) to open.\n");
          std::cout << help.toLocal8Bit().constData() << std::endl;
          return 0;
      }
  }

  MainWindow w;

  if (argc > 1) {
      w.processFile(QString::fromLocal8Bit(argv[1]));
  }

  w.show();

  return a.exec();
}

