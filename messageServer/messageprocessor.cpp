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

void MessageProcessor::personalMessageProcess(const QByteArray &data, ChatNetworkManager *manager)
{
    /*if(json["flag"].toString() == "group_voice_message" || json["flag"].toString() == "group_file_message") {
        json["flag"] = "group_message";
        groupMessageProcess(json,manager);
        return;
    } else if(json["flag"].toString() == "personal_voice_message" || json["flag"].toString() == "group_file_message") {
        json["flag"] = "personal_message";
    }*/
    chats::ChatMessage message;
    QProtobufSerializer serializer;
    message.deserialize(&serializer,data);

    quint64 receiver_id = message.receiverId();
    quint64 sender_id = message.senderId();

    message.setSenderLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id));
    message.setReceiverLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(receiver_id));

    quint64 dialog_id = DatabaseConnector::instance().getChatManager()->getOrCreateDialog(sender_id, receiver_id);
    quint64 message_id;
    QString content = message.content();
    QString fileUrl =  message.mediaUrl();

    message_id = DatabaseConnector::instance().getChatManager()->saveMessage(dialog_id, sender_id,receiver_id, content, fileUrl, "personal");
    message.setMessageId(message_id);
    sendMessageToActiveSockets(message.serialize(&serializer), "personal_message", sender_id, receiver_id, manager);
}

void MessageProcessor::sendMessageToActiveSockets(const QByteArray &data, const QString &flag, const quint64 &sender_id, const quint64 &receiver_id, ChatNetworkManager *manager)
{
    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getId() == sender_id || client->getId() == receiver_id) client->sendData(flag,data);
        //if(client->getId() == receiver_id && sender_id != receiver_id) client->sendData(flag,data);
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
        /*if(client->getId() == sender_id) {
            client->sendData(flag,data);// instead of sendToClient
            //sendToClient(client, json, false);
        }
        if(groupMembersIds.contains(client->getId()) && sender_id != client->getId()) {
            client->sendData(flag,data);// instead of sendToClient
            //sendToClient(client, json, false);
        }*/
    }
}

void MessageProcessor::groupMessageProcess(const QByteArray &data, ChatNetworkManager *manager)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::groupMessageProcess", "Method starts");

    QProtobufSerializer serializer;
    chats::ChatMessage message;
    if (!message.deserialize(&serializer, data)) {
        Logger::instance().log(Logger::ERROR, "messageprocessor.cpp::groupMessageProcess", "Failed to deserialize group message");
        return;
    }

    quint64 group_id = message.groupId();
    quint64 sender_id = message.senderId();
    QString content = message.content();
    QString fileUrl = message.mediaUrl();

    message.setSenderLogin(DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id));
    message.setSenderAvatarUrl(DatabaseConnector::instance().getUserManager()->getUserAvatar(sender_id));
    message.setGroupName(DatabaseConnector::instance().getGroupManager()->getGroupName(group_id));
    message.setGroupAvatarUrl(DatabaseConnector::instance().getGroupManager()->getGroupAvatar(group_id));
    message.setTimestamp(QDateTime::currentDateTime().toString(Qt::ISODate));

    quint64 message_id = DatabaseConnector::instance().getChatManager()->saveMessage(
        0, sender_id, group_id, content, fileUrl, "group");

    message.setMessageId(message_id);
    QList<int> groupMembersIds = DatabaseConnector::instance().getGroupManager()->getGroupMembers(group_id);
    sendGroupMessageToActiveSockets(message.serialize(&serializer), "group_message", groupMembersIds, manager);
}

void MessageProcessor::sendNewGroupAvatarUrlToActiveSockets(const QJsonObject &json, ChatNetworkManager *manager)
{
    QList<int> groupMembersIds = DatabaseConnector::instance().getGroupManager()->getGroupMembers(json["id"].toInt());
    QJsonObject jsonToSend = json;
    //dont forget add time to proto like json["time"] = QDateTime::currentDateTime().toString("HH:mm");
    //sendGroupMessageToActiveSockets(jsonToSend, manager, groupMembersIds);
}

QJsonObject MessageProcessor::createMessageJson(QJsonObject json, int message_id, int dialog_id)
{
    QJsonObject messageJson;
    messageJson["flag"] = "personal_message";
    messageJson["message_id"] = message_id;
    messageJson["message"] = json["message"];
    messageJson["dialog_id"] = dialog_id;

    messageJson["sender_login"] = json["sender_login"];
    messageJson["sender_id"] = json["sender_id"];
    messageJson["sender_avatar_url"] = json["sender_avatar_url"];

    messageJson["receiver_login"] = json["receiver_login"];
    messageJson["receiver_id"] = json["receiver_id"];
    messageJson["receiver_avatar_url"] = json["receiver_avatar_url"];

    messageJson["fileUrl"] = json["fileUrl"];
    messageJson["time"] = QDateTime::currentDateTime().toString("HH:mm");
    return messageJson;
}

void MessageProcessor::sendToClient(ClientHandler *client, QJsonObject& messageJson, bool isSender)
{

    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::sendToClient", "Method starts");
    if(isSender) {
        messageJson.remove("sender_login");
        messageJson.remove("sender_id");
        messageJson.remove("sender_avatar_url");
    } else {
        messageJson.remove("receiver_login");
        messageJson.remove("receiver_id");
        messageJson.remove("receiver_avatar_url");
    }
    client->sendJson(messageJson);
}
