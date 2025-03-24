#include "chatmanager.h"
#include "databaseconnector.h"


ChatManager::ChatManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector)
{}

QJsonObject ChatManager::getDialogInfo(const QJsonObject &json)
{
    QString userlogin = json["userlogin"].toString();
    int user_id = databaseConnector->getUserManager()->getUserId(userlogin);
    QList<int> dialogsIds = getUserDialogs(user_id);
    QJsonObject dialogsInfo;
    dialogsInfo["flag"] = "dialogs_info";

    QJsonArray dialogsInfoArray;
    for (int dialog_id : dialogsIds) {
        QJsonObject dialogsInfoJson;
        if(dialog_id == 0) continue;

        QMap<QString, QVariant> params;
        params[":dialog_id"] = dialog_id;
        params[":user_id"] = user_id;
        QSqlQuery query;
        if (!databaseConnector->executeQuery(query,"SELECT CASE WHEN user1_id = :user_id THEN user2_id ELSE user1_id END AS second_user_id FROM dialogs WHERE dialog_id = :dialog_id",params) && query.next()) {
            int second_user_id = query.value(0).toInt();

            QSqlQuery userQuery;
            QMap<QString, QVariant> userParams;
            userParams[":second_user_id"] = second_user_id;
            if (databaseConnector->executeQuery(userQuery, "SELECT * FROM users WHERE id_user = :second_user_id", userParams) && userQuery.next()) {
                int id_user = userQuery.value(0).toInt();
                QString username = userQuery.value(1).toString();
                QString userlogin = userQuery.value(2).toString();
                QString phone_number = userQuery.value(4).toString();
                QString avatar_url = userQuery.value(5).toString();
                QDateTime created_at = userQuery.value(6).toDateTime();

                dialogsInfoJson["user_id"] = id_user;
                dialogsInfoJson["username"] = username;
                dialogsInfoJson["userlogin"] = userlogin;
                dialogsInfoJson["phone_number"] = phone_number;
                dialogsInfoJson["avatar_url"] = avatar_url;
                dialogsInfoJson["created_at"] = created_at.toString();
            } else {
                qWarning() << "User with id: " << second_user_id << " not found";
            }
        } else {
            qWarning() << "Dialog with id:" << dialog_id << " not found";
        }

        dialogsInfoArray.append(dialogsInfoJson);
    }
    dialogsInfo["info"] = dialogsInfoArray;
    return dialogsInfo;
}

int ChatManager::getOrCreateDialog(int sender_id, int receiver_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user1"] = sender_id;
    params[":user2"] = receiver_id;
    databaseConnector->executeQuery(query, "SELECT dialog_id FROM dialogs WHERE (user1_id = :user1 AND user2_id = :user2) OR (user1_id = :user2 AND user2_id = :user1)",params);
    if (query.next()) {
        return query.value(0).toInt();
    } else {
        QMap<QString, QVariant> insertParams;
        insertParams[":user1"] = sender_id;
        insertParams[":user2"] = receiver_id;
        databaseConnector->executeQuery(query,"INSERT INTO dialogs (user1_id, user2_id) VALUES (:user1, :user2)",insertParams);
        return query.lastInsertId().toInt();
    }
}

QJsonObject ChatManager::updatingChatsProcess(QJsonObject json)
{
    QJsonArray jsonMessageArray;
    QJsonObject updatingJson;
    updatingJson["flag"] = "updating_chats";
    getUserMessages(json, jsonMessageArray);

    updatingJson["messages"] = jsonMessageArray;
    return updatingJson;
}

int ChatManager::saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &flag)
{
    QSqlQuery query;
    QString queryStr;
    QMap<QString, QVariant> params;
    if(flag == "personal") {
        params[":receiver_id"] = receiverId;
        params[":dialog_id"] = dialogId;
        queryStr = "INSERT INTO messages (sender_id, receiver_id, dialog_id, content, media_url) VALUES (:sender_id, :receiver_id, :dialog_id, :content, :fileUrl)";
    } else if (flag == "group") {
        params[":group_id"] = receiverId;
        queryStr = "INSERT INTO messages (sender_id, group_id, content, media_url) VALUES (:sender_id, :group_id, :content, :fileUrl)";
    }
    params[":sender_id"] = senderId;
    params[":content"] = message;
    params[":fileUrl"] = fileUrl;
    if (!databaseConnector->executeQuery(query,queryStr,params)) {
        qDebug() << "Error exec query:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

QJsonObject ChatManager::loadMessages(const QJsonObject &requestJson)
{
    int chat_id = requestJson["chat_id"].toInt();
    int user_id = requestJson["user_id"].toInt();
    int offset = requestJson["offset"].toInt();
    QString type = requestJson["type"].toString();

    int dialog_id = getOrCreateDialog(user_id,chat_id);

    QJsonArray jsonMessageArray;

    QSqlQuery query;
    QString queryStr;
    QMap<QString, QVariant> params;
    if(type == "personal") {
        params[":dialog_id"] = dialog_id;
        params[":offset"] = offset;
        queryStr = "SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset";
    } else if(type == "group") {
        params[":group_id"] = chat_id;
        params[":offset"] = chat_id;
        queryStr = "SELECT message_id, content, media_url, timestamp, sender_id FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset";
    }

    if (!databaseConnector->executeQuery(query,queryStr,params)) qDebug() << "Fail execute query (loadMessagesProcess): " << query.lastError().text();
    while (query.next()) {
        if (type == "personal") {
            appendMessageObject(query, jsonMessageArray);
        } else if (type == "group") {
            int message_id = query.value(0).toInt();
            QString content = query.value(1).toString();
            QString fileUrl = query.value(2).toString();
            QString timestamp = query.value(3).toString();
            int senderId = query.value(4).toInt();

            QJsonObject messageObject;
            messageObject["FullDate"] = timestamp;
            messageObject["fileUrl"] = fileUrl;
            messageObject["group_id"] = chat_id;
            messageObject["group_name"] = databaseConnector->getGroupManager()->getGroupName(chat_id);
            messageObject["sender_id"] = senderId;
            messageObject["sender_login"] = databaseConnector->getUserManager()->getUserLogin(senderId);
            messageObject["message_id"] = message_id;
            messageObject["str"] = content;
            messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

            jsonMessageArray.prepend(messageObject);
        }
    }
    QJsonObject response;
    response["flag"] = "load_messages";
    response["chat_name"] = requestJson["chat_name"];
    response["type"] = requestJson["type"];
    response["messages"] = jsonMessageArray;
    return response;
}

void ChatManager::getUserMessages(QJsonObject json, QJsonArray &jsonMessageArray)
{
    QString userlogin = json["userlogin"].toString();
    int user_id = databaseConnector->getUserManager()->getUserId(userlogin);
    if (user_id == -1) return;

    QList<int> dialogIds = getUserDialogs(user_id);
    for (int dialog_id : dialogIds) {
        QSqlQuery query;
        QMap<QString, QVariant> params;
        params[":dialog_id"] = dialog_id;

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50", params)) {
            qDebug() << "Error query execution (select message in getUserMessages):" << query.lastError().text();
            continue;
        }
        while (query.next()) {
            appendMessageObject(query, jsonMessageArray);
        }
    }
    QList<int> groupIds = databaseConnector->getGroupManager()->getUserGroups(user_id);
    for (int group_id : groupIds) {
        QSqlQuery query;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50", params)) {
            qDebug() << "Error query execution (select message in getUserMessages):" << query.lastError().text();
            continue;
        }

        while (query.next()) {
            int message_id = query.value(0).toInt();
            QString content = query.value(1).toString();
            QString fileUrl = query.value(2).toString();
            QString timestamp = query.value(3).toString();
            int senderId = query.value(4).toInt();

            QJsonObject messageObject;
            messageObject["FullDate"] = timestamp;
            messageObject["fileUrl"] = fileUrl;
            messageObject["group_id"] = group_id;
            messageObject["group_name"] = databaseConnector->getGroupManager()->getGroupName(group_id);
            messageObject["sender_id"] = senderId;
            messageObject["sender_login"] = databaseConnector->getUserManager()->getUserLogin(senderId);
            messageObject["message_id"] = message_id;
            messageObject["str"] = content;
            messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

            jsonMessageArray.prepend(messageObject);
        }
    }
}

QList<int> ChatManager::getUserDialogs(int user_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;
    QList<int> dialogIds;
    if (databaseConnector->executeQuery(query, "SELECT dialog_id FROM messages WHERE sender_id = :user_id OR receiver_id = :user_id", params)) {
        while (query.next()) {
            int dialogId = query.value(0).toInt();
            if (!dialogIds.contains(dialogId)) {
                dialogIds.append(dialogId);
            }
        }
    } else {
        qDebug() << "Error getting dialogs for user_id: " << user_id <<" with error: "<< query.lastError().text();
    }
    return dialogIds;
}

void ChatManager::appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();
    int receiverId = query.value(5).toInt();

    if (message_id + senderId + receiverId == 0) return;

    QJsonObject messageObject;
    messageObject["FullDate"] = timestamp;
    messageObject["receiver_login"] = databaseConnector->getUserManager()->getUserLogin(receiverId);
    messageObject["sender_login"] = databaseConnector->getUserManager()->getUserLogin(senderId);
    messageObject["receiver_id"] = receiverId;
    messageObject["sender_id"] = senderId;
    messageObject["message_id"] = message_id;
    messageObject["str"] = content;
    messageObject["fileUrl"] = fileUrl;
    messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

    jsonMessageArray.prepend(messageObject);
}
