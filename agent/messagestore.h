#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUuid>

struct StoredMessage {
    QUuid id;
    QString title;
    QString text;
    QDateTime sentUtc;
    QString senderIp;
    int replyPort = 0;
    QDateTime readUtc;
    QString readBy;
    bool receiptSent = false;
};

/// Хранилище в %ProgramData%\LanMessenger\messages.json.
/// Папку создаёт служба (SYSTEM), обычные пользователи имеют право только чтения.
class MessageStore
{
public:
    MessageStore();

    bool add(const StoredMessage &m); // false — такое сообщение уже есть
    QList<StoredMessage> unread() const;
    bool markRead(const QUuid &id, const QString &user);
    QList<StoredMessage> pendingReceipts() const;
    void markReceiptSent(const QUuid &id);

private:
    void save();
    QString m_path;
    QList<StoredMessage> m_items;
};
