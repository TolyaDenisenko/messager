#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LanMessenger");
    QCoreApplication::setApplicationName("Sender");
    MainWindow w;
    w.show();
    return app.exec();
}
