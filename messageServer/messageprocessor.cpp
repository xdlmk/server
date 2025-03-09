#include "messageprocessor.h"
#include "chatnetworkmanager.h"
#include "clienthandler.h"

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

    QString receiver_avatar_url = DatabaseManager::instance().getAvatarUrl(receiver_id);
    QString sender_avatar_url = DatabaseManager::instance().getAvatarUrl(sender_id);
    json["receiver_avatar_url"] = receiver_avatar_url;
    json["sender_avatar_url"] = sender_avatar_url;

    int dialog_id = DatabaseManager::instance().getOrCreateDialog(sender_id, receiver_id);
    int message_id;

    if(json.contains("fileUrl")) {
        QString fileUrl =  json["fileUrl"].toString();
        message_id = DatabaseManager::instance().saveMessageToDatabase(dialog_id, sender_id,receiver_id, message, fileUrl, "personal");
    } else {
        message_id = DatabaseManager::instance().saveMessageToDatabase(dialog_id, sender_id,receiver_id, message, "" , "personal");
    }
    sendMessageToActiveSockets(json, manager, message_id, dialog_id);
}

void MessageProcessor::sendMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, int message_id, int dialog_id)
{
    QString sender_login = json["sender_login"].toString();
    QString receiver_login = json["receiver_login"].toString();

    QJsonObject messageJson = createMessageJson(json, message_id, dialog_id);

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        if(client->getLogin() == sender_login) sendToClient(client, messageJson, true);
        if(client->getLogin() == receiver_login && sender_login != receiver_login) sendToClient(client, messageJson, false);
    }
}

void MessageProcessor::sendGroupMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, int message_id, QList<int> groupMembersIds)
{
    qDebug() << "sendGroupMessageToActiveSockets starts";
    int sender_id = json["sender_id"].toInt();
    qDebug() << "sender_id: " << sender_id;
    json["message_id"] = message_id;
    json["time"] = QDateTime::currentDateTime().toString("HH:mm");

    QList<ClientHandler*> clients = manager->getClients();
    for(ClientHandler *client : clients) {
        qDebug() << "client->getId() = " << client->getId();
        qDebug() << "groupMembersIds = " << groupMembersIds;
        if(client->getId() == sender_id) {
            qDebug() << "client->getId() == sender_id";
            sendToClient(client, json, true);
        }
        qDebug() << "contains = " << groupMembersIds.contains(client->getId());
        if(groupMembersIds.contains(client->getId()) && sender_id != client->getId()) {
            qDebug() << "groupMembersIds.contains(client->getId()) && sender_id != client->getId()";
            sendToClient(client, json, false);
        }
    }
}

void MessageProcessor::groupMessageProcess(QJsonObject &json, ChatNetworkManager *manager)
{
    qDebug() << "groupMessageProcess starts";
    int group_id = json["group_id"].toInt();
    int sender_id =  json["sender_id"].toInt();
    QString message =  json["message"].toString();
    QList<int> groupMembersIds = DatabaseManager::instance().getGroupMembers(group_id);
    int message_id = 0;

    if(json["flag"].toString() != "group_info"){
        QString sender_avatar_url = DatabaseManager::instance().getAvatarUrl(sender_id);
        json["sender_avatar_url"] = sender_avatar_url;

        if(json.contains("fileUrl")) {
            QString fileUrl =  json["fileUrl"].toString();
            message_id = DatabaseManager::instance().saveMessageToDatabase(0, sender_id,group_id, message, fileUrl, "group");
        } else {
            message_id = DatabaseManager::instance().saveMessageToDatabase(0, sender_id,group_id, message, "", "group");
        }
    }
    sendGroupMessageToActiveSockets(json, manager, message_id, groupMembersIds);
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

    qDebug() << "sendToClient starts";
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
