#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <QObject>

#include <QJsonObject>
#include <QJsonDocument>
#include <QList>

#include "Database/databaseconnector.h"
#include "Database/groupmanager.h"
#include "Database/chatmanager.h"

class ChatNetworkManager;
class ClientHandler;

class MessageProcessor : public QObject {
    Q_OBJECT
public:
    explicit MessageProcessor(QObject *parent = nullptr);
    static void personalMessageProcess(QJsonObject &json, ChatNetworkManager *manager);
    static void sendMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, int message_id, int dialog_id);
    static void sendGroupMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, QList<int> groupMembersIds);
    static void groupMessageProcess(QJsonObject &json,ChatNetworkManager *manager);
    static void sendNewGroupAvatarUrlToActiveSockets(const QJsonObject &json,ChatNetworkManager *manager);
private:
    static QJsonObject createMessageJson(QJsonObject json, int message_id, int dialog_id);
    static void sendToClient(ClientHandler *client, QJsonObject& messageJson, bool isSender);
};

#endif // MESSAGEPROCESSOR_H
