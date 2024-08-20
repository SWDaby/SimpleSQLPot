#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QDate>
#include <QTime>
#include <QFile>
#include <QDir>

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    Ui::Widget *ui;
    char *buf;
    QFile *file;
    QTime time;
    QDate date;
    QString log_normal, log_pot;
    QTcpServer *serverSocket;
    QList<QTcpSocket*> clientSocket;
    QMap<QTcpSocket*, QTcpSocket*> SQLsocket;
    QMap<char, QString> command;
    QHash<QString, int> failcount;
    QHash<QString, QTime> blacklist;

    int initialcommand();
    int log(QString log_string);
    int logpot(QString log_string);
    QString direction(QString ipaddr);

private slots:
    void on_pb_start_clicked();
    void on_pb_stop_clicked();
    void on_pb_restart_clicked();
    void acceptConnection();
    void readClient();
    void readSQLserver();
    void clientdisconnected();
};

#endif // WIDGET_H
