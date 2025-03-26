#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>

#include <QThread>

#include "chatnetworkmanager.h"
#include "messageprocessor.h"

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(quintptr handle,ChatNetworkManager *manager, QObject *parent = nullptr);

    bool checkSocket(QTcpSocket *socket);
    bool setIdentifiers(const QString& login,const int& id);
    QString getLogin();
    int getId();

    void sendJson(const QJsonObject &jsonToSend);
signals:
    void clientDisconnected(ClientHandler *client);
private slots:
    void readClient();
    void disconnectClient();
    void handleFlag(const QString &flag, QJsonObject &json, QTcpSocket *socket);
private:
    QString login;
    int id;
    QTcpSocket *socket;

    ChatNetworkManager * manager;
};

#endif // CLIENTHANDLER_H
