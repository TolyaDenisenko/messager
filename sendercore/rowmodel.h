#pragma once
#include <QAbstractTableModel>
#include <QDateTime>
#include <QJsonArray>
#include <QUuid>
#include <QVector>

struct Row {
    QUuid msgId;
    QDateTime sent;
    QString title;
    QString ip;
    QString machine;
    QString user;
    QString note;
    int delivery = 0; // 0 — отправка, 1 — доставлено, 2 — не доставлено
    bool read = false;
    QDateTime readAt;
};

/// Таблица статусов. Таблица (колонки) — для Widgets, роли — для QML.
class RowModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColSent, ColTitle, ColIp, ColMachine, ColUser, ColDelivery, ColRead, ColReadAt, ColNote, ColCount };
    enum Role {
        SortRole = Qt::UserRole + 1,
        IpRole,
        MachineRole,
        UserNameRole,
        TitleRole,
        SentRole,
        DeliveryRole,
        DeliveryTextRole,
        ReadRole,
        ReadTextRole,
        ReadAtRole,
        NoteRole
    };

    using QAbstractTableModel::QAbstractTableModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int append(const Row &r);
    QString ipAt(int row) const { return m_rows.at(row).ip; }
    void setDelivered(int row, bool ok, const QString &machine, const QString &error);
    bool markRead(const QUuid &msgId, const QString &ip, const QString &machine, const QString &user,
                  const QDateTime &at);
    void clear();

    int total() const { return m_rows.size(); }
    int deliveredCount() const;
    int failedCount() const;
    int readCount() const;

    QJsonArray toJson() const;
    void load(const QJsonArray &arr);

private:
    QVector<Row> m_rows;
};
