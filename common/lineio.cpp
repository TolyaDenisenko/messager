#include "lineio.h"

#include <memory>

namespace lm {

static constexpr int kMaxLine = 256 * 1024;

void serveLine(QIODevice *dev, const QString &peer, LineHandler handler, int timeoutMs)
{
    auto buf = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);

    auto onReady = [=]() {
        if (*done)
            return;
        buf->append(dev->readAll());
        if (buf->size() > kMaxLine) {
            *done = true;
            dev->close();
            return;
        }
        const int i = buf->indexOf('\n');
        if (i < 0)
            return;
        *done = true;
        const QByteArray reply = handler(buf->left(i), peer);
        dev->write(reply);
        dev->close(); // закрытие дожидается отправки буфера
    };
    QObject::connect(dev, &QIODevice::readyRead, dev, onReady);
    QTimer::singleShot(timeoutMs, dev, [dev]() {
        if (dev->isOpen())
            dev->close();
    });
    if (dev->bytesAvailable() > 0)
        onReady();
}

LineServer::LineServer(LineHandler handler, QObject *parent) : QObject(parent), m_handler(std::move(handler))
{
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *s = m_server.nextPendingConnection()) {
            connect(s, &QAbstractSocket::disconnected, s, &QObject::deleteLater);
            const QString peer = QHostAddress(s->peerAddress().toIPv4Address()).toString();
            serveLine(s, peer, m_handler);
        }
    });
}

bool LineServer::listen(quint16 port) { return m_server.listen(QHostAddress::AnyIPv4, port); }

// ---------------------------------------------------------------------------

LineRequest::LineRequest(const QString &host, quint16 port, const QByteArray &payload, int timeoutMs, QObject *parent)
    : QObject(parent), m_host(host), m_port(port), m_payload(payload)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(timeoutMs);
    connect(&m_timer, &QTimer::timeout, this, [this]() { finish(false, {}, QStringLiteral("Таймаут")); });
    connect(&m_sock, &QTcpSocket::connected, this, [this]() { m_sock.write(m_payload); });
    connect(&m_sock, &QTcpSocket::readyRead, this, [this]() {
        m_buf += m_sock.readAll();
        if (m_buf.size() > kMaxLine) {
            finish(false, {}, QStringLiteral("Ответ слишком большой"));
            return;
        }
        const int i = m_buf.indexOf('\n');
        if (i >= 0)
            finish(true, m_buf.left(i), {});
    });
    connect(&m_sock, &QAbstractSocket::errorOccurred, this, [this]() { finish(false, {}, m_sock.errorString()); });
    connect(&m_sock, &QTcpSocket::disconnected, this,
            [this]() { finish(false, {}, QStringLiteral("Соединение закрыто")); });
}

void LineRequest::start()
{
    m_timer.start();
    m_sock.connectToHost(m_host, m_port);
}

void LineRequest::finish(bool ok, const QByteArray &reply, const QString &error)
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    m_sock.abort();
    emit finished(ok, reply, error);
    deleteLater();
}

void LineRequest::send(const QString &host, quint16 port, const QByteArray &payload, int timeoutMs, QObject *context,
                       Callback cb)
{
    auto *r = new LineRequest(host, port, payload, timeoutMs, context);
    connect(r, &LineRequest::finished, context, [cb](bool ok, const QByteArray &reply, const QString &err) {
        cb(ok, reply, err);
    });
    r->start();
}

} // namespace lm
