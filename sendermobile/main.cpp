#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "senderengine.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LanMessenger");
    QCoreApplication::setApplicationName("Sender");
    QCoreApplication::setApplicationVersion("1.0.0");
    QQuickStyle::setStyle("Material");

    SenderEngine sender;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("sender", &sender);
    engine.rootContext()->setContextProperty("appVersion", QCoreApplication::applicationVersion());
    engine.loadFromModule("SenderMobile", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
