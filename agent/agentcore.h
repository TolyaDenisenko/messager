#pragma once
#include <QLocalServer>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <memory>

#include "lineio.h"
#include "messagestore.h"

/// Логика агента: приём сообщений по TCP, канал с UI-хелпером, отправка квитанций.
class AgentCore : public QObject
{
    Q_OBJECT
public:
    explicit AgentCore(QObject *parent = nullptr);
    bool start();

private:
    QByteArray key() const;
    QByteArray onNetwork(const QByteArray &line, const QString &peer);
    QByteArray onPipe(const QByteArray &line);
    void flushReceipts();

    std::unique_ptr<lm::LineServer> m_tcp;
    QLocalServer m_pipe;
    MessageStore m_store;
    QTimer m_timer;
    QSet<QUuid> m_inflight;
};
