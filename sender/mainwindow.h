#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>

#include "senderengine.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void onSend();

    SenderEngine m_engine;
    QLineEdit *m_range;
    QLineEdit *m_key;
    QLineEdit *m_title;
    QPlainTextEdit *m_body;
    QPushButton *m_sendBtn;
    QLabel *m_summary;
    QTableView *m_table;
};
