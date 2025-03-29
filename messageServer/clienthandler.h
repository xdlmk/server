#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>

#include <QThread>
#include <unordered_map>
#include <string_view>

#include "chatnetworkmanager.h"
#include "messageprocessor.h"

#include "../Utils/logger.h"

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

    Logger& logger;

    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // CLIENTHANDLER_H
