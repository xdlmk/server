#include "messageprocessor.h"

#include "chatnetworkmanager.h"
#include "clienthandler.h"

#include "Database/databaseconnector.h"
#include "Database/chatmanager.h"
#include "Database/usermanager.h"
#include "Database/groupmanager.h"

MessageProcessor::MessageProcessor(QObject *parent)
    : QObject{parent}
{}

QString MessageProcessor::personalMessageProcess(const QByteArray &data, ChatNetworkManager *manager)
{
    chats::ChatMessage message;
    QProtobufSerializer serializer;
    if (!message.deserialize(&serializer, data)) {
        Logger::instance().log(Logger::INFO, "messageprocessor.cpp::personalMessageProcess", "Failed to deserialize personal message");
        return QString("fd");
    }

    quint64 receiver_id = message.receiverId();
    quint64 sender_id = message.senderId();
    quint64 dialog_id = DatabaseConnector::instance().getChatManager()->getDialog(sender_id, receiver_id);
    if(dialog_id == 0){
        QByteArray sender_encrypted_session_key = message.senderEncryptedSessionKey();
        QByteArray receiver_encrypted_session_key = message.receiverEncryptedSessionKey();
        if(sender_encrypted_session_key != QByteArray() && receiver_encrypted_session_key != QByteArray()){
            dialog_id = DatabaseConnector::instance().getChatManager()->getOrCreateDialog(sender_id, receiver_id, sender_encrypted_session_key, receiver_encrypted_session_key);
        } else return QString("nd");
    } else {
        message.setSenderEncryptedSessionKey(DatabaseConnector::instance().getChatManager()->getEncryptedSessionKey(dialog_id, sender_id));
        message.setReceiverEncryptedSessionKey(DatabaseConnector::instance().getChatManager()->getEncryptedSessionKey(dialog_id, receiver_id));
    }

    QString content = message.content();
    QString fileUrl =  message.mediaUrl();
    QString special_type = message.specialType();

    quint64 message_id = DatabaseConnector::instance().getChatManager()->saveMessage(dialog_id, sender_id,receiver_id, content, fileUrl, special_type, "personal");
    if(message_id == 0) {
        return QString("nm");
    }

    message.setSenderLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id));
    message.setSenderAvatarUrl(DatabaseConnector::instance().getUserManager()->getUserAvatar(sender_id));
    message.setReceiverLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(receiver_id));
    message.setReceiverAvatarUrl(DatabaseConnector::instance().getUserManager()->getUserAvatar(receiver_id));
    message.setTimestamp(QDateTime::currentDateTime().toString(Qt::ISODate));

    message.setMessageId(message_id);
    sendMessageToActiveSockets(message.serialize(&serializer), "personal_message", sender_id, receiver_id, manager);

    return QString("t");
}

void MessageProcessor::sendMessageToActiveSockets(const QByteArray &data, const QString &flag, const quint64 &sender_id, const quint64 &receiver_id, ChatNetworkManager *manager)
{
    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getId() == sender_id || client->getId() == receiver_id) client->sendData(flag,data);
    }
}

void MessageProcessor::sendGroupMessageToActiveSockets(const QByteArray& data, const QString& flag, QList<int> groupMembersIds, ChatNetworkManager *manager)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::sendGroupMessageToActiveSockets", "Method starts");

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if (groupMembersIds.contains(client->getId())) {
            client->sendData(flag, data);
        }
    }
}

void MessageProcessor::groupMessageProcess(const QByteArray &data, ChatNetworkManager *manager)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::groupMessageProcess", "Method starts");

    QProtobufSerializer serializer;
    chats::ChatMessage message;
    if (!message.deserialize(&serializer, data)) {
        Logger::instance().log(Logger::INFO, "messageprocessor.cpp::groupMessageProcess", "Failed to deserialize group message");
        return;
    }

    quint64 group_id = message.groupId();
    quint64 sender_id = message.senderId();
    QString content = message.content();
    QString fileUrl = message.mediaUrl();
    QString special_type = message.specialType();

    message.setSenderLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id));
    message.setSenderAvatarUrl(DatabaseConnector::instance().getUserManager()->getUserAvatar(sender_id));
    message.setGroupName(DatabaseConnector::instance().getGroupManager()->getGroupName(group_id));
    message.setGroupAvatarUrl(DatabaseConnector::instance().getGroupManager()->getGroupAvatar(group_id));
    message.setTimestamp(QDateTime::currentDateTime().toString(Qt::ISODate));

    quint64 message_id = DatabaseConnector::instance().getChatManager()->saveMessage(
        0, sender_id, group_id, content, fileUrl, special_type, "group");

    if(message_id == 0) {
        // add processing
        return;
    }

    message.setMessageId(message_id);
    QList<int> groupMembersIds = DatabaseConnector::instance().getGroupManager()->getGroupMembers(group_id);
    sendGroupMessageToActiveSockets(message.serialize(&serializer), "group_message", groupMembersIds, manager);
}
