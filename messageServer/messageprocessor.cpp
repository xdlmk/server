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
        if(client->getId() == sender_id) client->sendData(flag,data);
        if(client->getId() == receiver_id && sender_id != receiver_id) client->sendData(flag,data);
    }
}

void MessageProcessor::sendGroupMessageToActiveSockets(const QString& flag, QByteArray data, int sender_id, ChatNetworkManager *manager, QList<int> groupMembersIds)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::sendGroupMessageToActiveSockets", "Method starts");

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getId() == sender_id) {
            client->sendData(flag,data);// instead of sendToClient
            //sendToClient(client, json, false);
        }
        if(groupMembersIds.contains(client->getId()) && sender_id != client->getId()) {
            client->sendData(flag,data);// instead of sendToClient
            //sendToClient(client, json, false);
        }
    }
}

void MessageProcessor::groupMessageProcess(QJsonObject &json, ChatNetworkManager *manager)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::groupMessageProcess", "Method starts");
    int group_id = json["group_id"].toInt();
    int sender_id =  json["sender_id"].toInt();
    QString message =  json["message"].toString();

    QList<int> groupMembersIds = DatabaseConnector::instance().getGroupManager()->getGroupMembers(group_id);
    int message_id = 0;

    if(json["flag"].toString() != "group_info"){
        QString sender_avatar_url = DatabaseConnector::instance().getUserManager()->getUserAvatar(sender_id);
        json["sender_avatar_url"] = sender_avatar_url;
        json["sender_login"] = DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id);
        json["group_name"] = DatabaseConnector::instance().getGroupManager()->getGroupName(group_id);

        if(json.contains("fileUrl")) {
            QString fileUrl =  json["fileUrl"].toString();
            message_id = DatabaseConnector::instance().getChatManager()->saveMessage(0, sender_id,group_id, message, fileUrl, "group");
        } else {
            message_id = DatabaseConnector::instance().getChatManager()->saveMessage(0, sender_id,group_id, message, "", "group");
        }
    }

    json["message_id"] = message_id;
    //dont forget add time to proto like json["time"] = QDateTime::currentDateTime().toString("HH:mm");
    //sendGroupMessageToActiveSockets(json, manager, groupMembersIds);
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
