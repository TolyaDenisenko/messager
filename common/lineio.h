#pragma once
#include <QHostAddress>
#include <QIODevice>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

namespace lm {

/// Получает строку-запрос (без '\n') и адрес собеседника, возвращает строку-ответ (с '\n').
using LineHandler = std::function<QByteArray(const QByteArray &line, const QString &peer)>;

/// Один запрос = одно соединение: прочитать строку, вызвать handler, записать ответ, закрыть.
/// Работает с QTcpSocket и QLocalSocket (любой QIODevice).
void serveLine(QIODevice *dev, const QString &peer, LineHandler handler, int timeoutMs = 10000);

/// TCP-сервер на serveLine.
class LineServer : public QObject
{
    Q_OBJECT
public:
    explicit LineServer(LineHandler handler, QObject *parent = nullptr);
    bool listen(quint16 port);
    QString errorString() const { return m_server.errorString(); }

private:
    QTcpServer m_server;
    LineHandler m_handler;
};

/// Асинхронный клиентский запрос «отправил строку — получил строку». Удаляет себя после завершения.
class LineRequest : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QByteArray &reply, const QString &error)>;

    static void send(const QString &host, quint16 port, const QByteArray &payload, int timeoutMs, QObject *context,
                     Callback cb);

signals:
    void finished(bool ok, const QByteArray &reply, const QString &error);

private:
    LineRequest(const QString &host, quint16 port, const QByteArray &payload, int timeoutMs, QObject *parent);
    void start();
    void finish(bool ok, const QByteArray &reply, const QString &error);

    QTcpSocket m_sock;
    QTimer m_timer;
    QString m_host;
    quint16 m_port;
    QByteArray m_payload;
    QByteArray m_buf;
    bool m_done = false;
};

} // namespace lm
