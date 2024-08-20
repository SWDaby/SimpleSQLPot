#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    initialcommand();
    QDir dir("log");//新建log文件夹
    if(!dir.exists(QCoreApplication::applicationDirPath()+"/logs"))
    {
        dir.mkdir(QCoreApplication::applicationDirPath()+"/logs");
    }
    buf = new char[16777215];
    serverSocket = new QTcpServer();
    connect(serverSocket, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
    ui->cb_state->setEnabled(false);
    ui->loging->setContextMenuPolicy(Qt::NoContextMenu);
    //ui->loging->appendHtml("<head><style>.honeypot{color:red;}</style></head>");
    on_pb_start_clicked();
}

Widget::~Widget()
{
    delete[] buf;
    delete ui;
}
void Widget::on_pb_start_clicked()
{
    if(!serverSocket->isListening())
    {
        if (serverSocket->listen(QHostAddress(ui->le__ListingAddr->text().split(":")[0]), ui->le__ListingAddr->text().split(":")[1].toUShort()))//程序自身监听端口
        {
            log("监听成功");
            ui->cb_state->setChecked(true);
        } else {
            log("端口监听失败");
            ui->cb_state->setChecked(false);
        }
    }
}

void Widget::on_pb_stop_clicked()
{
    if(serverSocket->isListening())
    {
        serverSocket->close();
        for(int i=0; i<clientSocket.length(); i++)
        {
            SQLsocket[clientSocket[i]]->close();
            clientSocket[i]->close();
        }
        ui->cb_state->setChecked(false);
        log("监听终止");
    }
    if(!ui->cb_holdlist->isChecked())
    {
        failcount.clear();
        blacklist.clear();
    }
}

void Widget::on_pb_restart_clicked()
{
    on_pb_stop_clicked();
    on_pb_start_clicked();
}


void Widget::acceptConnection()//链接初始化
{
    QTcpSocket *Currentsocket = serverSocket->nextPendingConnection();
    clientSocket.append(Currentsocket);
    SQLsocket[Currentsocket]= new QTcpSocket(this);
    SQLsocket[Currentsocket]->connectToHost(QHostAddress(ui->le_SQLAddr->text().split(":")[0]), ui->le_SQLAddr->text().split(":")[1].toUShort());//le_SQLAddr -> mysql地址
    connect(Currentsocket, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(Currentsocket, SIGNAL(disconnected()), this, SLOT(clientdisconnected()));
    connect(SQLsocket[Currentsocket], SIGNAL(readyRead()), this, SLOT(readSQLserver()));
    //log("A new connection:" + QString::number(int(Currentsocket) & 0x0000FFFF,16));
}

void Widget::readClient()//？？
{
    char certsucces[11] = {7,0,0,1,0,0,0,2,0,0,0};
    for(int i=0; i<clientSocket.length(); i++)
    {
        QString ipaddr = clientSocket[i]->peerAddress().toString();
        if(blacklist.contains(ipaddr))
        {
            if(blacklist[ipaddr].secsTo(time.currentTime()) >= ui->sb_ttl->value())
            {
                //处理过期黑名单
                failcount[ipaddr] = 0;
                blacklist.remove(ipaddr);
            }
        }

        if("Reject" == direction(ipaddr))
        {
            //黑名单最大尝试次数后拒绝连接
            clientSocket[i]->close();
            clientSocket.removeAll(clientSocket[i]);
            continue;
        }
        qint64 size = clientSocket[i]->read(buf,16777215);
        if(size == 0)   continue;

        if("Relay" == direction(ipaddr))
        {
            //转发消息
            SQLsocket[clientSocket[i]]->write(buf,size);
            if(buf[4] >= 0x00 && buf[4] <= 0x1C && buf[3] != 0x01)
            {
                log("[" + clientSocket[i]->peerAddress().toString()+ "] [" +command[buf[4]] + "] " + &buf[5]);
            }
        }
        if("HoneyPot" == direction(ipaddr))
        {
            //进入蜜罐
            certsucces[3] =buf[3]+1;
            clientSocket[i]->write(certsucces,sizeof(certsucces));
            if(buf[4] >= 0x00 && buf[4] <= 0x1C && buf[3] != 0x01)//写入独立log
            {
                logpot("[" + clientSocket[i]->peerAddress().toString()+ "] [" +command[buf[4]] + "] " + &buf[5]);
            }
        }
        for(unsigned int i=0; i<size; i++) buf[i]=0;
    }
}

void Widget::readSQLserver()
{
    for(int i=0; i<clientSocket.length(); i++)
    {
        qint64 size = SQLsocket[clientSocket[i]]->read(buf,16777215);
        if(size == 0)   continue;
        QString ipaddr = clientSocket[i]->peerAddress().toString();
        if((buf[3] == 2 && buf[4] == -1))
        {
            failcount[ipaddr] +=1;
            if(failcount[ipaddr] >= ui->sb_attempt->value())
            {
                blacklist[ipaddr] = time.currentTime();
            }
            log(ipaddr+"登录失败x"+QString::number(failcount[ipaddr]));
        }
        clientSocket[i]->write(buf,size);
        for(unsigned int i=0; i<size; i++) buf[i]=0;
    }
}

void Widget::clientdisconnected()
{
    for(int i=0; i<clientSocket.length(); i++)
    {
        for(unsigned int i=0; i<16777215; i++) buf[i]=0;
        if(clientSocket[i]->state() == QAbstractSocket::UnconnectedState)
        {
            //log("client:" + QString::number(int(clientSocket[i]) & 0x0000FFFF,16) + " disconnected.");
            SQLsocket[clientSocket[i]]->close();
            delete SQLsocket[clientSocket[i]];
            SQLsocket.remove(clientSocket[i]);
            clientSocket.removeOne(clientSocket[i]);
        }
    }
}


int Widget::log(QString log_string)//运行日志
{
    log_string = time.currentTime().toString()+ " " + log_string;
    if(!ui->cb_logpot->isChecked())
    {
        ui->loging->appendHtml("<font color=\"#00FF00\">" + log_string + "</font> ");
    }
    log_string+= "\n";
    if(ui->cb_log->isChecked())
    {
        QFile file(QCoreApplication::applicationDirPath()+"/logs/" + date.currentDate().toString("yyyy-MM-dd") + ".log");
        if(file.open(QIODevice::Append | QIODevice::Text))
        {
            file.write(log_string.toUtf8());
            file.close();
        }
    }
    log_normal += log_string;
    return log_string.length();
}

int Widget::logpot(QString log_string)//蜜罐内日志
{
    log_string = time.currentTime().toString()+ " " + log_string;
    ui->loging->appendHtml("<font color=\"#"+ui->le_color->text()+"\">" + log_string + "</font> ");
    log_string+= "\n";
    if(ui->cb_log->isChecked())
    {
        QFile file(QCoreApplication::applicationDirPath()+"/logs/" + date.currentDate().toString("yyyy-MM-dd") + "_honeypot.log");
        if(file.open(QIODevice::Append | QIODevice::Text))
        {
             file.write(log_string.toUtf8());
             file.close();
        }
    }
     log_pot += log_string;
     return log_string.length();

}

QString Widget::direction(QString ipaddr)
{
    if(blacklist.contains(ipaddr))
    {
        //黑名单内
        if(ui->rb_reject->isChecked())
        {
            //最大尝试次数后拒绝服务
            return "Reject";
        } else {
            //最大尝试次数后进入蜜罐
            return "HoneyPot";
        }
    } else {
        //黑名单外
        if(ui->cb_relay->isChecked())
        {
            //启用转发
            return "Relay";
        } else {
            //禁用转发, 进入蜜罐
            return "HoneyPot";
        }
    }
}

//const definition
int Widget::initialcommand()
{
    command.insert(0x00, "SLEEP");
    command.insert(0x01, "QUIT");
    command.insert(0x02, "INIT_DB");
    command.insert(0x03, "QUERY");
    command.insert(0x04, "FIELD_LIST");
    command.insert(0x05, "CREATE_DB");
    command.insert(0x06, "DROP_DB");
    command.insert(0x07, "REFRESH");
    command.insert(0x08, "SHUTDOWN");
    command.insert(0x09, "STATISTICS");
    command.insert(0x0A, "PROCESS_INFO");
    command.insert(0x0B, "CONNECT");
    command.insert(0x0C, "PROCESS_KILL");
    command.insert(0x0D, "DEBUG");
    command.insert(0x0E, "PING");
    command.insert(0x0F, "TIME");
    command.insert(0x10, "DELAYED_INSERT");
    command.insert(0x11, "CHANGE_USER");
    command.insert(0x12, "BINLOG_DUMP");
    command.insert(0x13, "TABLE_DUMP");
    command.insert(0x14, "CONNECT_OUT");
    command.insert(0x15, "REGISTER_SLAVE");
    command.insert(0x16, "STMT_PREPARE");
    command.insert(0x17, "STMT_EXECUTE");
    command.insert(0x18, "STMT_SEND_LONG_DATA");
    command.insert(0x19, "STMT_CLOSE");
    command.insert(0x1A, "STMT_RESET");
    command.insert(0x1B, "SET_OPTION");
    command.insert(0x1C, "STMT_FETCH");
    return 0;
}
