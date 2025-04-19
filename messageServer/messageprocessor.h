#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <QObject>

#include <QJsonObject>
#include <QJsonDocument>
#include <QList>

#include "../Utils/logger.h"

#include "chatMessage.qpb.h"
#include "QProtobufSerializer"

class ChatNetworkManager;
class ClientHandler;

class MessageProcessor : public QObject {
    Q_OBJECT
public:
    explicit MessageProcessor(QObject *parent = nullptr);
    static void personalMessageProcess(const QByteArray &data, ChatNetworkManager *manager);
    static void sendMessageToActiveSockets(const QByteArray &data, const QString &flag, const quint64 &sender_id, const quint64 &receiver_id, ChatNetworkManager *manager);
    static void sendGroupMessageToActiveSockets(const QString& flag, QByteArray data, int sender_id, ChatNetworkManager *manager, QList<int> groupMembersIds);
    static void groupMessageProcess(QJsonObject &json,ChatNetworkManager *manager);
    static void sendNewGroupAvatarUrlToActiveSockets(const QJsonObject &json,ChatNetworkManager *manager);
private:
    static QJsonObject createMessageJson(QJsonObject json, int message_id, int dialog_id);
    static void sendToClient(ClientHandler *client, QJsonObject& messageJson, bool isSender);
};

#endif // MESSAGEPROCESSOR_H
