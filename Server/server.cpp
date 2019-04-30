#include "server.h"
#define PORT 5000
#define MAXBUFSIZE 2000
#define SEQLIMIT INT_MAX-1

Server::Server(QObject *parent) : QTcpServer(parent)
{
    sequence = 0;
    signalMapper = new QSignalMapper(this);
    connect(signalMapper,SIGNAL(mapped(int)),this,SLOT(Incoming(int)));
    Start();
}

void Server::Start()
{
    if(!this->listen(QHostAddress::Any,PORT))
        return;
    connect(this,SIGNAL(newConnection()),this,SLOT(NewConnection()));
}

void Server::NewConnection()
{
    QTcpSocket *client = new QTcpSocket;
    client = nextPendingConnection();
    clientSockets.push_back(client);
    connect(client,SIGNAL(readyRead()),signalMapper,SLOT(map()));
    signalMapper->setMapping(client,clientSockets.size()-1);

    QString str = QString::number(sequence) + " " + document;
    client->write(str.toStdString().c_str(),str.size());
}

void Server::Incoming(int idx)
{
    QTcpSocket *client = clientSockets[idx];
    QString msg(client->read(MAXBUFSIZE));
    updateDocument(msg);
    msg = QString::number(++sequence) + " " + msg;
    if(sequence > SEQLIMIT)
        sequence = 0;

    std::string str = msg.toStdString();
    for(int i=0; i<clientSockets.size(); i++)
    {
        if(clientSockets[i] == client)
            clientSockets[i]->write(("1 "+str).c_str(),qint64(str.size()+2));
        else
            clientSockets[i]->write(("0 "+str).c_str(),qint64(str.size()+2));
    }
}

void Server::updateDocument(QString event)
{
    QString temp;
    int i;
    for(i = 0; event[i] != ' '; i++)
        temp.append(event[i]);

    int curPos = temp.toInt();
    if(event[++i] == '1')
    {
        int numChar = (event.mid(i+1)).toInt();
        document = document.left(curPos-numChar) + document.mid(curPos);
    }
    else
    {
        event = event.mid(i+1);
        document = document.left(curPos) + event + document.mid(curPos);
    }
}
