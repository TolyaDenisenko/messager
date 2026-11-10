#pragma once
#include <QObject>
#include <QSortFilterProxyModel>
#include <memory>

#include "lineio.h"
#include "rowmodel.h"

/// Движок отправителя без GUI: общий для ПК-версии (Widgets) и мобильной (QML).
class SenderEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged)
    Q_PROPERTY(QString range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY summaryChanged)
    Q_PROPERTY(int deliveredCount READ deliveredCount NOTIFY summaryChanged)
    Q_PROPERTY(int readCount READ readCount NOTIFY summaryChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY summaryChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString listenError READ listenError CONSTANT)
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)

public:
    explicit SenderEngine(QObject *parent = nullptr);
    ~SenderEngine() override;

    QString key() const { return m_key; }
    void setKey(const QString &k);
    QString range() const { return m_range; }
    void setRange(const QString &r);
    QString summary() const { return m_summary; }
    bool busy() const { return m_active > 0; }
    int totalCount() const { return m_model.total(); }
    int deliveredCount() const { return m_model.deliveredCount(); }
    int readCount() const { return m_model.readCount(); }
    int failedCount() const { return m_model.failedCount(); }
    QString listenError() const { return m_listenError; }
    QAbstractItemModel *model() { return &m_sorted; }

    /// Возвращает текст ошибки валидации или пустую строку, если рассылка запущена.
    Q_INVOKABLE QString send(const QString &range, const QString &title, const QString &text);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void save();

signals:
    void keyChanged();
    void rangeChanged();
    void summaryChanged();
    void busyChanged();

private:
    struct Batch;
    void pump(const std::shared_ptr<Batch> &b);
    void updateSummary();
    QByteArray onReceipt(const QByteArray &line, const QString &peer);
    QString settingsPath() const;

    RowModel m_model;
    QSortFilterProxyModel m_sorted;
    std::unique_ptr<lm::LineServer> m_server;
    QString m_key, m_range, m_summary, m_listenError;
    int m_active = 0;
};
