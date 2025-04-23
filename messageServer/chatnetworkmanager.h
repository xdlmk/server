#ifndef CHATNETWORKMANAGER_H
#define CHATNETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "../Utils/logger.h"

class ClientHandler;

class ChatNetworkManager : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatNetworkManager(QObject *parent = nullptr);
    QList<ClientHandler*> getClients();
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
signals:
    void saveFileToDatabase(const QString &fileUrl);
    void setAvatarInDatabase(const QString &avatarUrl, const int &user_id);
    void setGroupAvatarInDatabase(const QString &avatarUrl, const int &group_id);
    void personalMessageProcess(const QByteArray &data, ChatNetworkManager *manager);
    void groupMessageProcess(const QByteArray &data, ChatNetworkManager *manager);
    void createGroup(const QByteArray& data);
private:
    QList<ClientHandler*> clients;
    void removeClient(ClientHandler *client);

    Logger& logger;
};

#endif // CHATNETWORKMANAGER_H
