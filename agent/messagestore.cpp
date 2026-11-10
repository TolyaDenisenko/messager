#include "messagestore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimeZone>
#include <algorithm>

static qint64 ms(const QDateTime &d) { return d.isValid() ? d.toMSecsSinceEpoch() : 0; }
static QDateTime fromMs(qint64 v) { return v > 0 ? QDateTime::fromMSecsSinceEpoch(v, QTimeZone::UTC) : QDateTime(); }

MessageStore::MessageStore()
{
    const QString base = qEnvironmentVariable("ProgramData", QStringLiteral("C:/ProgramData"));
    m_path = base + "/LanMessenger/messages.json";

    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        StoredMessage m;
        m.id = QUuid::fromString(o["id"].toString());
        m.title = o["title"].toString();
        m.text = o["text"].toString();
        m.sentUtc = fromMs(o["sent"].toInteger());
        m.senderIp = o["senderIp"].toString();
        m.replyPort = o["replyPort"].toInt();
        m.readUtc = fromMs(o["read"].toInteger());
        m.readBy = o["readBy"].toString();
        m.receiptSent = o["receiptSent"].toBool();
        m_items.append(m);
    }
}

void MessageStore::save()
{
    const QDateTime limit = QDateTime::currentDateTimeUtc().addDays(-30);
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                 [&](const StoredMessage &m) { return m.receiptSent && m.readUtc < limit; }),
                  m_items.end());

    QJsonArray arr;
    for (const auto &m : std::as_const(m_items)) {
        QJsonObject o;
        o["id"] = m.id.toString(QUuid::WithoutBraces);
        o["title"] = m.title;
        o["text"] = m.text;
        o["sent"] = QJsonValue(ms(m.sentUtc));
        o["senderIp"] = m.senderIp;
        o["replyPort"] = m.replyPort;
        o["read"] = QJsonValue(ms(m.readUtc));
        o["readBy"] = m.readBy;
        o["receiptSent"] = m.receiptSent;
        arr.append(o);
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

bool MessageStore::add(const StoredMessage &m)
{
    for (const auto &x : std::as_const(m_items))
        if (x.id == m.id)
            return false;
    m_items.append(m);
    save();
    return true;
}

QList<StoredMessage> MessageStore::unread() const
{
    QList<StoredMessage> r;
    for (const auto &m : m_items)
        if (!m.readUtc.isValid())
            r.append(m);
    std::sort(r.begin(), r.end(), [](const auto &a, const auto &b) { return a.sentUtc < b.sentUtc; });
    return r;
}

bool MessageStore::markRead(const QUuid &id, const QString &user)
{
    for (auto &m : m_items) {
        if (m.id != id)
            continue;
        if (!m.readUtc.isValid()) {
            m.readUtc = QDateTime::currentDateTimeUtc();
            m.readBy = user;
            save();
        }
        return true;
    }
    return false;
}

QList<StoredMessage> MessageStore::pendingReceipts() const
{
    QList<StoredMessage> r;
    for (const auto &m : m_items)
        if (m.readUtc.isValid() && !m.receiptSent)
            r.append(m);
    return r;
}

void MessageStore::markReceiptSent(const QUuid &id)
{
    for (auto &m : m_items)
        if (m.id == id) {
            m.receiptSent = true;
            save();
            return;
        }
}
