#include "chatmanager.h"

#include "databaseconnector.h"


ChatManager::ChatManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{}

QList<messages::DialogInfoItem> ChatManager::getDialogInfo(const int &user_id)
{
    QList<messages::DialogInfoItem> dialogsInfoList;

    if (!databaseConnector->getUserManager()->userIdCheck(user_id)) {
        return dialogsInfoList;
    }

    QList<int> dialogsIds = getUserDialogs(user_id);

    for (int dialog_id : dialogsIds) {
        if (dialog_id == 0) continue;

        QMap<QString, QVariant> params;
        params[":dialog_id"] = dialog_id;
        params[":user_id"] = user_id;
        QSqlQuery query;

        if (databaseConnector->executeQuery(query, "SELECT CASE WHEN user1_id = :user_id THEN user2_id ELSE user1_id END AS second_user_id FROM dialogs WHERE dialog_id = :dialog_id", params)) {
            if (query.next()) {
                int second_user_id = query.value(0).toInt();

                QSqlQuery userQuery;
                QMap<QString, QVariant> userParams;
                userParams[":second_user_id"] = second_user_id;

                if (databaseConnector->executeQuery(userQuery, "SELECT * FROM users WHERE id_user = :second_user_id", userParams) && userQuery.next()) {
                    messages::DialogInfoItem dialogItem;
                    dialogItem.setUserId(userQuery.value(0).toULongLong());
                    dialogItem.setUsername(userQuery.value(1).toString());
                    dialogItem.setUserlogin(userQuery.value(2).toString());
                    dialogItem.setPhoneNumber(userQuery.value(4).toString());
                    dialogItem.setAvatarUrl(userQuery.value(5).toString());
                    dialogItem.setCreatedAt(userQuery.value(6).toDateTime().toString());
                    dialogsInfoList.append(dialogItem);
                } else {
                    logger.log(Logger::DEBUG,"chatmanager.cpp::getDialogInfo", "User with id: " + QString::number(second_user_id) + " not found");
                }
            } else {
                logger.log(Logger::DEBUG,"chatmanager.cpp::getDialogInfo", "Dialog with id:" + QString::number(dialog_id) + " not found in database");
            }
        } else {
            logger.log(Logger::DEBUG,"chatmanager.cpp::getDialogInfo", "Query exec return false: " + query.lastError().text());
        }
    }

    return dialogsInfoList;
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

chats::UpdatingChatsResponse ChatManager::updatingChatsProcess(const quint64 &user_id)
{
    chats::UpdatingChatsResponse response;
    bool failed = false;
    QList<chats::ChatMessage> messages = getUserMessages(user_id, failed);
    if(failed){
        response.setStatus("error");
    } else {
        response.setMessages(messages);
    }
    return response;
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
        logger.log(Logger::WARN,"chatmanager.cpp::saveMessage", "Error exec query: " + query.lastError().text());
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

    int dialog_id = 0;
    if(type == "personal"){
        dialog_id = getOrCreateDialog(user_id,chat_id);
    }

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

    if (!databaseConnector->executeQuery(query,queryStr,params)) logger.log(Logger::WARN,"chatmanager.cpp::loadMessagesProcess", "Error exec query: " + query.lastError().text());
    while (query.next()) {
        if (type == "personal") {
            //appendMessageObject(query, jsonMessageArray); // use generatePersonalMessageObject
        } else if (type == "group") {
            // use generateGroupMessageObject
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

QList<chats::ChatMessage> ChatManager::getUserMessages(const quint64 user_id, bool &failed)
{
    if (!databaseConnector->getUserManager()->userIdCheck(user_id)) {
        failed = true;
        return QList<chats::ChatMessage>();
    }
    QList<chats::ChatMessage> messages;

    QList<int> dialogIds = getUserDialogs(user_id);
    for (int dialog_id : dialogIds) {
        QSqlQuery query;
        QMap<QString, QVariant> params;
        params[":dialog_id"] = dialog_id;

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50", params)) {
            logger.log(Logger::WARN,"chatmanager.cpp::getUserMessages", "Error exec query: " + query.lastError().text());
            continue;
        }
        while (query.next()) {
            messages.prepend(generatePersonalMessageObject(query));
        }
    }
    QList<int> groupIds = databaseConnector->getGroupManager()->getUserGroups(user_id);
    for (int group_id : groupIds) {
        QSqlQuery query;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50", params)) {
            logger.log(Logger::WARN,"chatmanager.cpp::getUserMessages", "Error exec query: " + query.lastError().text());
            continue;
        }

        while (query.next()) {
            chats::ChatMessage message = generateGroupMessageObject(query);
            message.setGroupId(group_id);
            message.setGroupName(databaseConnector->getGroupManager()->getGroupName(group_id));
            messages.prepend(message);
        }
    }
    return messages;
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
        logger.log(Logger::WARN,"chatmanager.cpp::getUserDialogs", "Error getting dialogs for user_id: " + QString::number(user_id) + " with error: " + query.lastError().text());
    }
    return dialogIds;
}

chats::ChatMessage ChatManager::generatePersonalMessageObject(QSqlQuery &query)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();
    int receiverId = query.value(5).toInt();

    if (message_id + senderId + receiverId == 0) return chats::ChatMessage();

    chats::ChatMessage message;
    message.setMessageId(message_id);
    message.setMediaUrl(fileUrl);
    message.setTimestamp(timestamp);
    message.setReceiverId(receiverId);
    message.setSenderId(senderId);
    message.setSenderLogin(databaseConnector->getUserManager()->getUserLogin(senderId));
    message.setReceiverLogin(databaseConnector->getUserManager()->getUserLogin(receiverId));
    message.setContent(content);

    return message;
}

chats::ChatMessage ChatManager::generateGroupMessageObject(QSqlQuery &query)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();

    chats::ChatMessage message;
    message.setMessageId(message_id);
    message.setMediaUrl(fileUrl);
    message.setTimestamp(timestamp);
    message.setSenderId(senderId);
    message.setSenderLogin(databaseConnector->getUserManager()->getUserLogin(senderId));
    message.setContent(content);

    return message;
}
