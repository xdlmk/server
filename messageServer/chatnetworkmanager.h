#ifndef CHATNETWORKMANAGER_H
#define CHATNETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "Database/databaseconnector.h"

class ClientHandler;

class ChatNetworkManager : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatNetworkManager(QObject *parent = nullptr);
    QList<ClientHandler*> getClients();
public slots:
    void setIdentifiersForClient(QTcpSocket *socket,const QString& login,const int& id);
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
signals:
    void saveFileToDatabase(const QString &fileUrl);
    void setAvatarInDatabase(const QString &avatarUrl, const int &user_id);
    void setGroupAvatarInDatabase(const QString &avatarUrl, const int &group_id);
    void personalMessageProcess(QJsonObject &json,ChatNetworkManager *manager);
    void createGroup(const QJsonObject& createGroupJson);
    void sendNewGroupAvatarUrlToActiveSockets(const QJsonObject& json, ChatNetworkManager *manager);
private:
    QList<ClientHandler*> clients;
    void removeClient(ClientHandler *client);
};

#endif // CHATNETWORKMANAGER_H
