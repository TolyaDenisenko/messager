// LanMessengerUI: запускается при входе пользователя (HKLM\...\Run), работает скрыто,
// опрашивает службу агента и показывает непрочитанные сообщения.
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QJsonArray>
#include <QLabel>
#include <QLocalSocket>
#include <QLockFile>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "protocol.h"

using namespace lm;

static bool g_sessionEnding = false;

class MessageDialog : public QDialog
{
public:
    MessageDialog(const QString &title, const QString &text, const QDateTime &sent) : QDialog(nullptr)
    {
        // Без кнопки закрытия, поверх всех окон
        setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
        setWindowTitle(QStringLiteral("Сообщение"));
        setMinimumSize(480, 280);
        resize(520, 320);

        auto *lay = new QVBoxLayout(this);
        auto *head = new QLabel(title.isEmpty() ? QStringLiteral("Сообщение") : title);
        QFont f = head->font();
        f.setPointSize(f.pointSize() + 4);
        f.setBold(true);
        head->setFont(f);
        head->setWordWrap(true);
        lay->addWidget(head);

        auto *when = new QLabel(QStringLiteral("Отправлено: ") + sent.toLocalTime().toString("dd.MM.yyyy HH:mm"));
        when->setStyleSheet("color: gray;");
        lay->addWidget(when);

        auto *body = new QTextEdit;
        body->setReadOnly(true); // редактирование запрещено
        body->setPlainText(text);
        lay->addWidget(body, 1);

        auto *btn = new QPushButton(QStringLiteral("Прочитано"));
        btn->setDefault(true);
        btn->setMinimumHeight(36);
        connect(btn, &QPushButton::clicked, this, [this]() {
            m_confirmed = true;
            accept();
        });
        lay->addWidget(btn);
    }

    bool confirmed() const { return m_confirmed; }

    void reject() override {} // Esc не закрывает
    void closeEvent(QCloseEvent *e) override
    {
        if (m_confirmed || g_sessionEnding)
            QDialog::closeEvent(e);
        else
            e->ignore(); // Alt+F4 не закрывает
    }

private:
    bool m_confirmed = false;
};

/// Синхронный короткий запрос к службе агента.
static QJsonObject pipeCall(const QJsonObject &req, bool *ok)
{
    *ok = false;
    QLocalSocket s;
    s.connectToServer(kPipeName);
    if (!s.waitForConnected(500))
        return {};
    s.write(encodeLine(req));
    QByteArray buf;
    while (!buf.contains('\n')) {
        if (!s.waitForReadyRead(1500))
            return {};
        buf += s.readAll();
    }
    return parseLine(buf.left(buf.indexOf('\n')), ok);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QObject::connect(&app, &QGuiApplication::commitDataRequest, [](QSessionManager &) { g_sessionEnding = true; });

    // Один экземпляр на сессию пользователя
    QLockFile lock(QDir::temp().filePath("lanmessenger-ui.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0))
        return 0;

    bool busy = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (busy)
            return;
        busy = true;
        bool ok = false;
        const QJsonObject resp = pipeCall(QJsonObject{{"cmd", "get"}}, &ok);
        if (ok && resp["ok"].toBool()) {
            const auto msgs = resp["messages"].toArray();
            for (const auto &v : msgs) {
                const auto o = v.toObject();
                MessageDialog dlg(o["title"].toString(), o["text"].toString(),
                                  QDateTime::fromMSecsSinceEpoch(o["sent"].toInteger()));
                dlg.show();
                dlg.raise();
                dlg.activateWindow();
                dlg.exec();
                if (!dlg.confirmed())
                    break;
                bool ok2 = false;
                pipeCall(QJsonObject{{"cmd", "read"},
                                     {"id", o["id"].toString()},
                                     {"user", qEnvironmentVariable("USERNAME")}},
                         &ok2);
            }
        }
        busy = false;
    });
    timer.start(3000);
    return app.exec();
}
