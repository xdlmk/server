#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(QObject *parent = nullptr);
signals:
    void clientDisconnected(ClientHandler* client);
private slots:
    void readClient();
    void disconnectClient();
    void sendJson(const QJsonDocument &doc);
private:
    QTcpSocket *socket;
};

#endif // CLIENTHANDLER_H
