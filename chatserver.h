#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>

class ChatServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);

signals:

    // QTcpServer interface
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;

private slots:
    void readClient();
    void disconnectClient();
    void SendToClient(QJsonDocument doc,QTcpSocket* socket);

private:
    QTcpSocket *socket;
    QByteArray data;
    QList <QTcpSocket*> clients;
};

#endif // CHATSERVER_H
