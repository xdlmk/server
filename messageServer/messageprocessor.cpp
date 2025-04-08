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

void MessageProcessor::personalMessageProcess(QJsonObject &json,ChatNetworkManager *manager)
{
    if(json.contains("group_id")) {
        json["flag"] = "group_message";
        groupMessageProcess(json,manager);
        return;
    }
    int receiver_id = json["receiver_id"].toInt();
    int sender_id =  json["sender_id"].toInt();
    QString message =  json["message"].toString();


    QString receiver_avatar_url = DatabaseConnector::instance().getUserManager()->getUserAvatar(receiver_id);
    QString sender_avatar_url = DatabaseConnector::instance().getUserManager()->getUserAvatar(sender_id);
    json["receiver_avatar_url"] = receiver_avatar_url;
    json["sender_avatar_url"] = sender_avatar_url;
    json["sender_login"] = DatabaseConnector::instance().getUserManager()->getUserLogin(sender_id);
    json["receiver_login"] = DatabaseConnector::instance().getUserManager()->getUserLogin(receiver_id);

    int dialog_id = DatabaseConnector::instance().getChatManager()->getOrCreateDialog(sender_id, receiver_id);
    int message_id;

    if(json.contains("fileUrl")) {
        QString fileUrl =  json["fileUrl"].toString();
        message_id = DatabaseConnector::instance().getChatManager()->saveMessage(dialog_id, sender_id,receiver_id, message, fileUrl, "personal");
    } else {
        message_id = DatabaseConnector::instance().getChatManager()->saveMessage(dialog_id, sender_id,receiver_id, message, "" , "personal");
    }
    sendMessageToActiveSockets(json, manager, message_id, dialog_id);
}

void MessageProcessor::sendMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, int message_id, int dialog_id)
{
    int receiver_id = json["receiver_id"].toInt();
    int sender_id =  json["sender_id"].toInt();

    QJsonObject messageJson = createMessageJson(json, message_id, dialog_id);

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getId() == sender_id) sendToClient(client, messageJson, true);
        if(client->getId() == receiver_id && sender_id != receiver_id) sendToClient(client, messageJson, false);
    }
}

void MessageProcessor::sendGroupMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, QList<int> groupMembersIds)
{
    Logger::instance().log(Logger::INFO,"messageprocessor.cpp::sendGroupMessageToActiveSockets", "Method starts");
    int sender_id;
    if(json.contains("sender_id")){
    sender_id = json["sender_id"].toInt();
    } else sender_id = -1;
    json["time"] = QDateTime::currentDateTime().toString("HH:mm");

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getId() == sender_id) {
            sendToClient(client, json, false);
        }
        if(groupMembersIds.contains(client->getId()) && sender_id != client->getId()) {
            sendToClient(client, json, false);
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
    sendGroupMessageToActiveSockets(json, manager, groupMembersIds);
}

void MessageProcessor::sendNewGroupAvatarUrlToActiveSockets(const QJsonObject &json, ChatNetworkManager *manager)
{
    QList<int> groupMembersIds = DatabaseConnector::instance().getGroupManager()->getGroupMembers(json["id"].toInt());
    QJsonObject jsonToSend = json;
    sendGroupMessageToActiveSockets(jsonToSend, manager, groupMembersIds);
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
