#include "include/mainwindow.h"
#include "include/config/appconfig.h"
#include "include/localizationmanager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  a.setWindowIcon(QIcon(QStringLiteral(":/drawer/icons/spritestudio.png")));
  QCoreApplication::setOrganizationName(QStringLiteral("SpriteStudio"));
  QCoreApplication::setApplicationName(QStringLiteral("SpriteStudio"));

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
