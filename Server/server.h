#ifndef SERVER_H
#define SERVER_H
#include <QtNetwork>
#include <QVector>

class Server : public QTcpServer
{
    Q_OBJECT

    QVector<QTcpSocket*> clientSockets;
    QSignalMapper* signalMapper;
    QString document;
    int sequence;
    void DeleteWords(int,int);
    void InsertWords(int,QString);
    void updateDocument(QString);
public:
    Server(QObject *parent = 0);
    void Start();
public slots:
    void NewConnection();
    void Incoming(int);
};

#endif // SERVER_H
