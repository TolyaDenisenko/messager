#include "senderengine.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include "iprange.h"
#include "protocol.h"

using namespace lm;

struct SenderEngine::Batch {
    QList<int> pending;
    int inflight = 0;
    QByteArray payload;
    QByteArray key;
};

static constexpr int kConcurrency = 100;
static constexpr int kConnectTimeoutMs = 3000;

SenderEngine::SenderEngine(QObject *parent) : QObject(parent)
{
    m_sorted.setSourceModel(&m_model);
    m_sorted.setSortRole(RowModel::SortRole);
    m_sorted.setDynamicSortFilter(true);
    m_sorted.sort(0, Qt::DescendingOrder); // новые сверху

    QFile f(settingsPath());
    if (f.open(QIODevice::ReadOnly)) {
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        m_key = o["key"].toString();
        m_range = o["range"].toString();
        m_model.load(o["rows"].toArray());
    }

    m_server = std::make_unique<LineServer>([this](const QByteArray &l, const QString &p) { return onReceipt(l, p); });
    if (!m_server->listen(kServerPort))
        m_listenError = QStringLiteral("Не удалось открыть порт %1 для приёма «прочитано»: %2")
                            .arg(kServerPort)
                            .arg(m_server->errorString());
    updateSummary();
}

SenderEngine::~SenderEngine() { save(); }

QString SenderEngine::settingsPath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/sender.json";
}

void SenderEngine::save()
{
    QJsonObject o;
    o["key"] = m_key;
    o["range"] = m_range;
    o["rows"] = m_model.toJson();
    QSaveFile f(settingsPath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void SenderEngine::setKey(const QString &k)
{
    if (k == m_key)
        return;
    m_key = k;
    emit keyChanged();
}

void SenderEngine::setRange(const QString &r)
{
    if (r == m_range)
        return;
    m_range = r;
    emit rangeChanged();
}

void SenderEngine::updateSummary()
{
    m_summary = QStringLiteral("Всего: %1  ·  Доставлено: %2  ·  Прочитано: %3  ·  Не доставлено: %4")
                    .arg(m_model.total())
                    .arg(m_model.deliveredCount())
                    .arg(m_model.readCount())
                    .arg(m_model.failedCount());
    emit summaryChanged();
}

QString SenderEngine::send(const QString &range, const QString &title, const QString &text)
{
    const QString key = m_key.trimmed();
    if (key.isEmpty())
        return QStringLiteral("Введите общий ключ (тот же, что вшит в MSI агента)");
    if (text.trimmed().isEmpty())
        return QStringLiteral("Введите текст сообщения");
    if (text.size() > kMaxTextLength)
        return QStringLiteral("Сообщение слишком длинное (максимум %1 символов)").arg(kMaxTextLength);

    QString err;
    const QStringList ips = parseIpRanges(range, &err);
    if (ips.isEmpty())
        return err;
    setRange(range);

    Packet p;
    p.type = "message";
    p.id = QUuid::createUuid();
    p.title = title.trimmed();
    p.text = text;
    p.sentUtc = QDateTime::currentDateTimeUtc();
    p.replyPort = kServerPort;
    p.sign(key.toUtf8());

    auto batch = std::make_shared<Batch>();
    batch->payload = encodeLine(p.toJson());
    batch->key = key.toUtf8();
    for (const QString &ip : ips) {
        Row r;
        r.msgId = p.id;
        r.sent = p.sentUtc;
        r.title = p.title.isEmpty() ? text.left(60) : p.title;
        r.ip = ip;
        batch->pending << m_model.append(r);
    }

    ++m_active;
    emit busyChanged();
    updateSummary();
    pump(batch);
    return {};
}

void SenderEngine::pump(const std::shared_ptr<Batch> &b)
{
    while (b->inflight < kConcurrency && !b->pending.isEmpty()) {
        const int row = b->pending.takeFirst();
        ++b->inflight;
        LineRequest::send(
            m_model.ipAt(row), kAgentPort, b->payload, kConnectTimeoutMs, this,
            [this, b, row](bool ok, const QByteArray &reply, const QString &error) {
                --b->inflight;
                bool delivered = false;
                QString machine, note = error;
                if (ok) {
                    bool parsed = false;
                    const Packet a = Packet::fromJson(parseLine(reply, &parsed));
                    if (parsed && a.ok && a.verify(b->key)) {
                        delivered = true;
                        machine = a.machine;
                        note.clear();
                    } else {
                        note = QStringLiteral("Агент отклонил сообщение (неверный ключ?)");
                    }
                }
                m_model.setDelivered(row, delivered, machine, note);
                updateSummary();
                if (b->pending.isEmpty() && b->inflight == 0) {
                    --m_active;
                    emit busyChanged();
                    save();
                } else {
                    pump(b);
                }
            });
    }
}

QByteArray SenderEngine::onReceipt(const QByteArray &line, const QString &peer)
{
    const QByteArray k = m_key.trimmed().toUtf8();
    bool parsed = false;
    const Packet p = Packet::fromJson(parseLine(line, &parsed));

    Packet ack;
    ack.type = "ack";
    if (parsed && p.type == "read" && p.verify(k)) {
        m_model.markRead(p.id, peer, p.machine, p.user, p.readUtc);
        ack.ok = true; // подтверждаем и неизвестные id (история очищена), чтобы агент не повторял
        updateSummary();
        save();
    } else {
        ack.error = "rejected";
    }
    ack.sign(k);
    return encodeLine(ack.toJson());
}

void SenderEngine::clearHistory()
{
    m_model.clear();
    updateSummary();
    save();
}
