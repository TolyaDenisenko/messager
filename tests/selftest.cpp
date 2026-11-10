// Самотест полного цикла на localhost. Перед запуском: LanMessengerAgent.exe --console с LM_SHARED_KEY=testkey
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QTimer>
#include <cstdio>

#include "protocol.h"
#include "senderengine.h"

using namespace lm;

static int g_fail = 0;
static void check(bool c, const char *what)
{
    std::printf("%s  %s\n", c ? "OK  " : "FAIL", what);
    if (!c) ++g_fail;
}
static void waitFor(std::function<bool()> cond, int ms)
{
    QElapsedTimer t; t.start();
    while (!cond() && t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}
static QJsonObject pipe(const QJsonObject &req)
{
    QLocalSocket s; s.connectToServer(kPipeName);
    if (!s.waitForConnected(1000)) return {};
    s.write(encodeLine(req));
    QByteArray b;
    while (!b.contains('\n')) { if (!s.waitForReadyRead(2000)) return {}; b += s.readAll(); }
    return parseLine(b.left(b.indexOf('\n')));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LanMessengerSelfTest");
    QCoreApplication::setApplicationName("Sender");

    SenderEngine e;
    e.clearHistory();
    check(e.listenError().isEmpty(), "отправитель слушает порт 5601");

    // неверный диапазон / пустой ключ
    e.setKey("");
    check(!e.send("127.0.0.1", "", "x").isEmpty(), "пустой ключ отклонён");
    e.setKey("testkey");
    check(!e.send("999.1.1.1", "", "x").isEmpty(), "неверный IP отклонён");

    // отправка: 127.0.0.1 (агент есть) и 127.0.0.2 (агент слушает 0.0.0.0 — тоже достижим), 127.0.0.1-1 дубль не нужен
    check(e.send("127.0.0.1", "Тест", "Привет, мир").isEmpty(), "рассылка запущена");
    waitFor([&] { return !e.busy(); }, 8000);
    auto *m = e.model();
    check(m->rowCount() == 1 && m->index(0, 0).data(RowModel::DeliveryRole).toInt() == 1, "доставлено");
    check(m->index(0, 0).data(RowModel::ReadTextRole).toString() == QStringLiteral("Не прочитано"), "статус: не прочитано");

    // «пользователь» читает через pipe
    auto r = pipe({{"cmd", "get"}});
    auto msgs = r["messages"].toArray();
    check(msgs.size() == 1 && msgs[0].toObject()["text"].toString() == QStringLiteral("Привет, мир"), "UI получил сообщение");
    auto rr = pipe({{"cmd", "read"}, {"id", msgs[0].toObject()["id"]}, {"user", "tester"}});
    check(rr["ok"].toBool(), "UI отметил прочитанным");
    check(pipe({{"cmd", "get"}})["messages"].toArray().isEmpty(), "прочитанное больше не показывается");

    waitFor([&] { return m->index(0, 0).data(RowModel::ReadRole).toBool(); }, 8000);
    check(m->index(0, 0).data(RowModel::ReadTextRole).toString() == QStringLiteral("Прочитано"), "квитанция: прочитано");
    check(m->index(0, 0).data(RowModel::UserNameRole).toString() == "tester", "имя пользователя в таблице");

    // недоступный хост
    check(e.send("127.0.0.1, 192.0.2.55", "", "второе").isEmpty(), "вторая рассылка");
    waitFor([&] { return !e.busy(); }, 10000);
    int failed = 0;
    for (int i = 0; i < m->rowCount(); ++i) failed += m->index(i, 0).data(RowModel::DeliveryRole).toInt() == 2;
    check(failed == 1, "недоступный 192.0.2.55 — не доставлено");

    // неверный ключ
    e.setKey("wrong");
    check(e.send("127.0.0.1", "", "с неверным ключом").isEmpty(), "рассылка с неверным ключом");
    waitFor([&] { return !e.busy(); }, 8000);
    failed = 0;
    for (int i = 0; i < m->rowCount(); ++i) failed += m->index(i, 0).data(RowModel::DeliveryRole).toInt() == 2;
    check(failed == 2, "неверный ключ — агент отклонил");

    e.clearHistory();
    std::printf(g_fail ? "\nПРОВАЛЕНО: %d\n" : "\nВСЕ ПРОВЕРКИ ПРОЙДЕНЫ\n", g_fail);
    return g_fail ? 1 : 0;
}
