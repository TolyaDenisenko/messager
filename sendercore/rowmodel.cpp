#include "rowmodel.h"

#include <QColor>
#include <QJsonObject>
#include <QTimeZone>

static qint64 ms(const QDateTime &d) { return d.isValid() ? d.toMSecsSinceEpoch() : 0; }
static QDateTime fromMs(qint64 v) { return v > 0 ? QDateTime::fromMSecsSinceEpoch(v, QTimeZone::UTC) : QDateTime(); }
static QString local(const QDateTime &d)
{
    return d.isValid() ? d.toLocalTime().toString("dd.MM.yyyy HH:mm:ss") : QString();
}

static QString deliveryText(const Row &r)
{
    switch (r.delivery) {
    case 1:
        return QStringLiteral("Доставлено");
    case 2:
        return QStringLiteral("Не доставлено");
    default:
        return QStringLiteral("Отправка…");
    }
}

static QString readText(const Row &r)
{
    if (r.delivery != 1)
        return QStringLiteral("—");
    return r.read ? QStringLiteral("Прочитано") : QStringLiteral("Не прочитано");
}

int RowModel::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : m_rows.size(); }
int RowModel::columnCount(const QModelIndex &p) const { return p.isValid() ? 0 : ColCount; }

QVariant RowModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(idx.row());

    switch (role) {
    case SortRole:
        return qint64(ms(r.sent)) * 1000000 - idx.row();
    case IpRole:
        return r.ip;
    case MachineRole:
        return r.machine;
    case UserNameRole:
        return r.user;
    case TitleRole:
        return r.title;
    case SentRole:
        return local(r.sent);
    case DeliveryRole:
        return r.delivery;
    case DeliveryTextRole:
        return deliveryText(r);
    case ReadRole:
        return r.read;
    case ReadTextRole:
        return readText(r);
    case ReadAtRole:
        return local(r.readAt);
    case NoteRole:
        return r.note;
    default:
        break;
    }

    if (role == Qt::DisplayRole) {
        switch (idx.column()) {
        case ColSent: return local(r.sent);
        case ColTitle: return r.title;
        case ColIp: return r.ip;
        case ColMachine: return r.machine;
        case ColUser: return r.user;
        case ColDelivery: return deliveryText(r);
        case ColRead: return readText(r);
        case ColReadAt: return local(r.readAt);
        case ColNote: return r.note;
        }
    } else if (role == Qt::ForegroundRole) {
        if (idx.column() == ColDelivery) {
            if (r.delivery == 1) return QColor(0, 128, 0);
            if (r.delivery == 2) return QColor(200, 30, 30);
        } else if (idx.column() == ColRead && r.delivery == 1) {
            return r.read ? QColor(0, 128, 0) : QColor(200, 120, 0);
        }
    }
    return {};
}

QVariant RowModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList h = {QStringLiteral("Отправлено"), QStringLiteral("Сообщение"), QStringLiteral("IP"),
                                  QStringLiteral("Компьютер"),  QStringLiteral("Пользователь"),
                                  QStringLiteral("Доставка"),   QStringLiteral("Прочтение"),
                                  QStringLiteral("Время прочтения"), QStringLiteral("Примечание")};
    return h.value(section);
}

QHash<int, QByteArray> RowModel::roleNames() const
{
    return {{IpRole, "ip"},
            {MachineRole, "machine"},
            {UserNameRole, "userName"},
            {TitleRole, "title"},
            {SentRole, "sent"},
            {DeliveryRole, "delivery"},
            {DeliveryTextRole, "deliveryText"},
            {ReadRole, "isRead"},
            {ReadTextRole, "readText"},
            {ReadAtRole, "readAt"},
            {NoteRole, "note"}};
}

int RowModel::append(const Row &r)
{
    const int i = m_rows.size();
    beginInsertRows({}, i, i);
    m_rows.append(r);
    endInsertRows();
    return i;
}

void RowModel::setDelivered(int row, bool ok, const QString &machine, const QString &error)
{
    if (row < 0 || row >= m_rows.size())
        return;
    Row &r = m_rows[row];
    r.delivery = ok ? 1 : 2;
    r.machine = machine;
    r.note = error;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

bool RowModel::markRead(const QUuid &msgId, const QString &ip, const QString &machine, const QString &user,
                        const QDateTime &at)
{
    int found = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &r = m_rows[i];
        if (r.msgId != msgId)
            continue;
        if (r.ip == ip) { found = i; break; }
        if (found < 0 && !machine.isEmpty() && r.machine.compare(machine, Qt::CaseInsensitive) == 0)
            found = i; // у агента несколько сетевых карт — совпадаем по имени компьютера
    }
    if (found < 0)
        return false;
    Row &r = m_rows[found];
    r.read = true;
    r.readAt = at.isValid() ? at : QDateTime::currentDateTimeUtc();
    if (!user.isEmpty())
        r.user = user;
    emit dataChanged(index(found, 0), index(found, ColCount - 1));
    return true;
}

void RowModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int RowModel::deliveredCount() const
{
    int n = 0;
    for (const auto &r : m_rows) n += r.delivery == 1;
    return n;
}
int RowModel::failedCount() const
{
    int n = 0;
    for (const auto &r : m_rows) n += r.delivery == 2;
    return n;
}
int RowModel::readCount() const
{
    int n = 0;
    for (const auto &r : m_rows) n += r.read;
    return n;
}

QJsonArray RowModel::toJson() const
{
    QJsonArray arr;
    const int skip = qMax(0, int(m_rows.size()) - 5000); // храним последние 5000 записей
    for (int i = skip; i < m_rows.size(); ++i) {
        const Row &r = m_rows[i];
        QJsonObject o;
        o["msgId"] = r.msgId.toString(QUuid::WithoutBraces);
        o["sent"] = QJsonValue(ms(r.sent));
        o["title"] = r.title;
        o["ip"] = r.ip;
        o["machine"] = r.machine;
        o["user"] = r.user;
        o["note"] = r.note;
        o["delivery"] = r.delivery == 0 ? 2 : r.delivery; // прерванная отправка считается недоставленной
        o["read"] = r.read;
        o["readAt"] = QJsonValue(ms(r.readAt));
        arr.append(o);
    }
    return arr;
}

void RowModel::load(const QJsonArray &arr)
{
    beginResetModel();
    m_rows.clear();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        Row r;
        r.msgId = QUuid::fromString(o["msgId"].toString());
        r.sent = fromMs(o["sent"].toInteger());
        r.title = o["title"].toString();
        r.ip = o["ip"].toString();
        r.machine = o["machine"].toString();
        r.user = o["user"].toString();
        r.note = o["note"].toString();
        r.delivery = o["delivery"].toInt();
        r.read = o["read"].toBool();
        r.readAt = fromMs(o["readAt"].toInteger());
        m_rows.append(r);
    }
    endResetModel();
}
