#include "mainwindow.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("LAN Messenger — отправка сообщений"));
    resize(1150, 700);

    auto *central = new QWidget;
    auto *v = new QVBoxLayout(central);

    auto *form = new QFormLayout;
    m_range = new QLineEdit(m_engine.range());
    m_range->setPlaceholderText(QStringLiteral("192.168.1.1-192.168.1.254  или  192.168.1.0/24  (можно несколько через запятую)"));
    m_key = new QLineEdit(m_engine.key());
    m_key->setPlaceholderText(QStringLiteral("Общий ключ (задаётся при сборке MSI)"));
    m_key->setEchoMode(QLineEdit::Password);
    m_title = new QLineEdit;
    m_title->setPlaceholderText(QStringLiteral("Заголовок (необязательно)"));
    m_body = new QPlainTextEdit;
    m_body->setPlaceholderText(QStringLiteral("Текст сообщения"));
    m_body->setMaximumHeight(130);
    form->addRow(QStringLiteral("Диапазон IP:"), m_range);
    form->addRow(QStringLiteral("Ключ:"), m_key);
    form->addRow(QStringLiteral("Заголовок:"), m_title);
    form->addRow(QStringLiteral("Сообщение:"), m_body);
    v->addLayout(form);

    auto *bar = new QHBoxLayout;
    m_sendBtn = new QPushButton(QStringLiteral("Отправить"));
    m_sendBtn->setMinimumHeight(32);
    auto *clearBtn = new QPushButton(QStringLiteral("Очистить историю"));
    m_summary = new QLabel;
    bar->addWidget(m_sendBtn);
    bar->addWidget(clearBtn);
    bar->addWidget(m_summary, 1);
    v->addLayout(bar);

    m_table = new QTableView;
    m_table->setModel(m_engine.model());
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    v->addWidget(m_table, 1);
    setCentralWidget(central);

    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSend);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, windowTitle(), QStringLiteral("Очистить всю историю отправок?"))
            == QMessageBox::Yes)
            m_engine.clearHistory();
    });
    connect(m_key, &QLineEdit::textChanged, &m_engine, &SenderEngine::setKey);
    connect(&m_engine, &SenderEngine::summaryChanged, this, [this]() { m_summary->setText(m_engine.summary()); });
    connect(&m_engine, &SenderEngine::busyChanged, this, [this]() { m_sendBtn->setEnabled(!m_engine.busy()); });
    m_summary->setText(m_engine.summary());

    if (!m_engine.listenError().isEmpty())
        statusBar()->showMessage(m_engine.listenError());
}

void MainWindow::onSend()
{
    const QString err = m_engine.send(m_range->text(), m_title->text(), m_body->toPlainText());
    if (!err.isEmpty())
        QMessageBox::warning(this, windowTitle(), err);
}
