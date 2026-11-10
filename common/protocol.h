#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUuid>

namespace lm {

constexpr quint16 kAgentPort = 5600;   // агент слушает сообщения от отправителя
constexpr quint16 kServerPort = 5601;  // отправитель слушает квитанции «прочитано»
constexpr int kMaxTextLength = 20000;
inline constexpr const char *kPipeName = "LanMessengerUI";
inline constexpr const char *kRegistryKey = "HKEY_LOCAL_MACHINE\\SOFTWARE\\LanMessenger";

/// Сетевой пакет: message (отправитель -> агент), read (агент -> отправитель), ack (ответ).
/// Каждый пакет подписывается HMAC-SHA256 общим ключом.
struct Packet {
    QString type;
    QUuid id;
    QString title;
    QString text;
    QDateTime sentUtc;
    QDateTime readUtc;
    int replyPort = 0;
    QString machine;
    QString user;
    bool ok = false;
    QString error;
    QString sig;

    QJsonObject toJson() const;
    static Packet fromJson(const QJsonObject &o);

    void sign(const QByteArray &key);
    bool verify(const QByteArray &key) const;

private:
    QString compute(const QByteArray &key) const;
};

/// Один JSON-объект на строку.
QByteArray encodeLine(const QJsonObject &o);
QJsonObject parseLine(const QByteArray &line, bool *ok = nullptr);

} // namespace lm
