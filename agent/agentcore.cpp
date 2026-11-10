#include "agentcore.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QLocalSocket>
#include <QSettings>
#include <QTimeZone>

#include "protocol.h"

using namespace lm;

AgentCore::AgentCore(QObject *parent) : QObject(parent) {}

QByteArray AgentCore::key() const
{
    // Общий ключ пишет MSI: HKLM\SOFTWARE\LanMessenger\SharedKey
    QSettings s(kRegistryKey, QSettings::NativeFormat);
    const QByteArray k = s.value("SharedKey").toString().toUtf8();
    return k.isEmpty() ? qgetenv("LM_SHARED_KEY") : k; // переменная — только для отладки в консоли
}

bool AgentCore::start()
{
    m_tcp = std::make_unique<LineServer>([this](const QByteArray &l, const QString &p) { return onNetwork(l, p); });
    if (!m_tcp->listen(kAgentPort)) {
        qCritical() << "Не удалось открыть порт" << kAgentPort << m_tcp->errorString();
        return false;
    }

    // Канал для UI-хелпера в сессии пользователя; доступ всем (содержимое сообщений не секретно,
    // а подделать «прочитано» может только локальный пользователь на своей же машине).
    QLocalServer::removeServer(kPipeName);
    m_pipe.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!m_pipe.listen(kPipeName)) {
        qCritical() << "Не удалось создать pipe" << m_pipe.errorString();
        return false;
    }
    connect(&m_pipe, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *s = m_pipe.nextPendingConnection()) {
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
            serveLine(s, {}, [this](const QByteArray &l, const QString &) { return onPipe(l); }, 5000);
        }
    });

    m_timer.setInterval(20000); // повтор неотправленных квитанций
    connect(&m_timer, &QTimer::timeout, this, &AgentCore::flushReceipts);
    m_timer.start();
    flushReceipts();
    return true;
}

QByteArray AgentCore::onNetwork(const QByteArray &line, const QString &peer)
{
    const QByteArray k = key();
    bool ok = false;
    const Packet p = Packet::fromJson(parseLine(line, &ok));

    Packet reply;
    reply.type = "ack";
    reply.machine = QSysInfo::machineHostName();

    if (ok && p.type == "message" && p.verify(k) && p.text.size() <= kMaxTextLength) {
        StoredMessage m;
        m.id = p.id;
        m.title = p.title;
        m.text = p.text;
        m.sentUtc = p.sentUtc;
        m.senderIp = peer;
        m.replyPort = p.replyPort > 0 ? p.replyPort : kServerPort;
        m_store.add(m); // повторная отправка того же id — не дублируется, но тоже подтверждается
        reply.id = p.id;
        reply.ok = true;
    } else {
        reply.error = "rejected";
    }
    reply.sign(k);
    return encodeLine(reply.toJson());
}

QByteArray AgentCore::onPipe(const QByteArray &line)
{
    bool ok = false;
    const QJsonObject req = parseLine(line, &ok);
    QJsonObject resp;
    resp["ok"] = false;
    if (ok && req["cmd"].toString() == "get") {
        QJsonArray arr;
        for (const auto &m : m_store.unread()) {
            QJsonObject o;
            o["id"] = m.id.toString(QUuid::WithoutBraces);
            o["title"] = m.title;
            o["text"] = m.text;
            o["sent"] = QJsonValue(m.sentUtc.toMSecsSinceEpoch());
            arr.append(o);
        }
        resp["ok"] = true;
        resp["messages"] = arr;
    } else if (ok && req["cmd"].toString() == "read") {
        resp["ok"] = m_store.markRead(QUuid::fromString(req["id"].toString()), req["user"].toString());
        flushReceipts();
    }
    return encodeLine(resp);
}

void AgentCore::flushReceipts()
{
    const QByteArray k = key();
    for (const auto &m : m_store.pendingReceipts()) {
        if (m_inflight.contains(m.id))
            continue;
        m_inflight.insert(m.id);

        Packet p;
        p.type = "read";
        p.id = m.id;
        p.readUtc = m.readUtc;
        p.machine = QSysInfo::machineHostName();
        p.user = m.readBy;
        p.sign(k);

        const QUuid id = m.id;
        LineRequest::send(m.senderIp, quint16(m.replyPort), encodeLine(p.toJson()), 5000, this,
                          [this, id, k](bool okReq, const QByteArray &reply, const QString &) {
                              m_inflight.remove(id);
                              if (!okReq)
                                  return; // отправитель недоступен — повторим через 20 с
                              bool parsed = false;
                              const Packet a = Packet::fromJson(parseLine(reply, &parsed));
                              if (parsed && a.ok && a.verify(k))
                                  m_store.markReceiptSent(id);
                          });
    }
}
