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

#include "include/mainwindow.h"
#include "include/config/appconfig.h"
#include "include/localizationmanager.h"
#include "include/license/licensemanager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(QStringLiteral(":/drawer/icons/spritestudio.png")));
  QCoreApplication::setOrganizationName(QStringLiteral("SpriteStudio"));
  QCoreApplication::setApplicationName(QStringLiteral("SpriteStudio"));

  SpriteStudio::LicenseManager::setToolType(SpriteStudio::ToolType::GUI);

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
