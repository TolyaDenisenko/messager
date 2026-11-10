#include "protocol.h"

#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QTimeZone>

namespace lm {

static qint64 ms(const QDateTime &d) { return d.isValid() ? d.toMSecsSinceEpoch() : 0; }
static QDateTime fromMs(qint64 v)
{
    return v > 0 ? QDateTime::fromMSecsSinceEpoch(v, QTimeZone::UTC) : QDateTime();
}

QJsonObject Packet::toJson() const
{
    QJsonObject o;
    o["type"] = type;
    o["id"] = id.toString(QUuid::WithoutBraces);
    o["title"] = title;
    o["text"] = text;
    o["sent"] = QJsonValue(ms(sentUtc));
    o["read"] = QJsonValue(ms(readUtc));
    o["replyPort"] = replyPort;
    o["machine"] = machine;
    o["user"] = user;
    o["ok"] = ok;
    o["error"] = error;
    o["sig"] = sig;
    return o;
}

Packet Packet::fromJson(const QJsonObject &o)
{
    Packet p;
    p.type = o["type"].toString();
    p.id = QUuid::fromString(o["id"].toString());
    p.title = o["title"].toString();
    p.text = o["text"].toString();
    p.sentUtc = fromMs(o["sent"].toInteger());
    p.readUtc = fromMs(o["read"].toInteger());
    p.replyPort = o["replyPort"].toInt();
    p.machine = o["machine"].toString();
    p.user = o["user"].toString();
    p.ok = o["ok"].toBool();
    p.error = o["error"].toString();
    p.sig = o["sig"].toString();
    return p;
}

QString Packet::compute(const QByteArray &key) const
{
    const QChar sep(0x1f);
    const QString canonical = type + sep + id.toString(QUuid::WithoutBraces) + sep + title + sep + text + sep
        + QString::number(ms(sentUtc)) + sep + QString::number(ms(readUtc)) + sep + QString::number(replyPort)
        + sep + machine + sep + user + sep + (ok ? "1" : "0");
    return QString::fromLatin1(
        QMessageAuthenticationCode::hash(canonical.toUtf8(), key, QCryptographicHash::Sha256).toBase64());
}

void Packet::sign(const QByteArray &key) { sig = compute(key); }

bool Packet::verify(const QByteArray &key) const
{
    if (key.isEmpty() || sig.isEmpty())
        return false;
    const QByteArray a = sig.toUtf8(), b = compute(key).toUtf8();
    if (a.size() != b.size())
        return false;
    int diff = 0; // сравнение за постоянное время
    for (int i = 0; i < a.size(); ++i)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

QByteArray encodeLine(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject parseLine(const QByteArray &line, bool *ok)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(line, &err);
    const bool good = err.error == QJsonParseError::NoError && doc.isObject();
    if (ok)
        *ok = good;
    return good ? doc.object() : QJsonObject();
}

} // namespace lm
