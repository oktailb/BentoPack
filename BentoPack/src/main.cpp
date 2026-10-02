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
#include <QApplication>
#include <iostream>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i) {
      QString arg = QString::fromLocal8Bit(argv[i]);
      if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
#if defined(Q_OS_WIN)
          if (AttachConsole(ATTACH_PARENT_PROCESS)) {
              (void)freopen("CONOUT$", "w", stdout);
              (void)freopen("CONOUT$", "w", stderr);
          }
#endif
          std::cout << "BentoPack Studio v1.0.0 (Qt 6 - GUI Engine)" << std::endl;
          return 0;
      }
      if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
#if defined(Q_OS_WIN)
          if (AttachConsole(ATTACH_PARENT_PROCESS)) {
              (void)freopen("CONOUT$", "w", stdout);
              (void)freopen("CONOUT$", "w", stderr);
          }
#endif
          std::cout << "Usage: bentopack [file.bento]\n"
                    << "Options:\n"
                    << "  -v, --version  Display version information\n"
                    << "  -h, --help     Display this help text" << std::endl;
          return 0;
      }
  }

  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(QStringLiteral(":/drawer/icons/bentopack.png")));
  QCoreApplication::setOrganizationName(QStringLiteral("BentoPack"));
  QCoreApplication::setApplicationName(QStringLiteral("BentoPack"));

  // Load configuration and initialize localization dynamically
  AppConfig::instance().load();
  LocalizationManager::instance().init();

  MainWindow w;

  if (argc > 1) {
      w.processFile(QString::fromLocal8Bit(argv[1]));
  }

  w.show();

  return a.exec();
}

