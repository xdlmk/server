#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <QObject>

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
    static QString personalMessageProcess(const QByteArray &data, ChatNetworkManager *manager);
    static void sendMessageToActiveSockets(const QByteArray &data, const QString &flag, const quint64 &sender_id, const quint64 &receiver_id, ChatNetworkManager *manager);
    static void sendDataToActiveSocket(const QByteArray &data, const QString &flag, const quint64 &user_id, ChatNetworkManager *manager);
    static void sendGroupMessageToActiveSockets(const QByteArray& data, const QString& flag, QList<quint64> groupMembersIds, ChatNetworkManager *manager);
    static void groupMessageProcess(const QByteArray &data, ChatNetworkManager *manager);
};

#endif // MESSAGEPROCESSOR_H
