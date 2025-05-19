#include "chatmanager.h"

#include "databaseconnector.h"


ChatManager::ChatManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{}

QList<chats::DialogInfoItem> ChatManager::getDialogInfo(const int &user_id)
{
    QList<chats::DialogInfoItem> dialogsInfoList;

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

        if (databaseConnector->executeQuery(query, "SELECT "
                                                   "CASE WHEN user1_id = :user_id THEN user2_id ELSE user1_id END AS second_user_id, "
                                                   "CASE WHEN user1_id = :user_id THEN encrypted_session_key_user1 ELSE encrypted_session_key_user2 END AS encrypted_session_key "
                                                   "FROM dialogs WHERE dialog_id = :dialog_id", params)) {
            if (query.next()) {
                quint64 second_user_id = query.value(0).toInt();
                QByteArray encryptedSessionKey = query.value(1).toByteArray();

                QSqlQuery userQuery;
                QMap<QString, QVariant> userParams;
                userParams[":second_user_id"] = second_user_id;

                if (databaseConnector->executeQuery(userQuery, "SELECT * FROM users WHERE id_user = :second_user_id", userParams) && userQuery.next()) {
                    chats::DialogInfoItem dialogItem;
                    dialogItem.setUserId(userQuery.value(0).toULongLong());
                    dialogItem.setUsername(userQuery.value(1).toString());
                    dialogItem.setUserlogin(userQuery.value(2).toString());
                    dialogItem.setPhoneNumber(userQuery.value(4).toString());
                    dialogItem.setAvatarUrl(userQuery.value(5).toString());
                    dialogItem.setCreatedAt(userQuery.value(6).toDateTime().toString());
                    dialogItem.setEncryptedSessionKey(encryptedSessionKey);
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

bool ChatManager::markMessage(const quint64 &message_id, const quint64 &reader_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":message_id"] = message_id;
    params[":user_id"] = reader_id;
    if(!databaseConnector->executeQuery(query,
                                         "INSERT INTO message_reads (message_id, user_id) VALUES (:message_id, :user_id)",
                                         params)) return false;
    return true;
}

QPair<quint64, quint64> ChatManager::getSenderReceiverByMessageId(const quint64 &messageId)
{
    quint64 senderId = 0;
    quint64 receiverId = 0;

    QMap<QString, QVariant> params;
    params[":message_id"] = messageId;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,
                                         "SELECT sender_id, receiver_id FROM messages WHERE message_id = :message_id",
                                         params)) return qMakePair(0ULL, 0ULL);
    if(query.next()) {
        senderId = query.value(0).toULongLong();
        receiverId = query.value(1).toULongLong();
    } else return qMakePair(0ULL, 0ULL);

    return qMakePair(senderId, receiverId);
}

QByteArray ChatManager::createDialog(const QByteArray &dialogData)
{
    QProtobufSerializer serializer;
    chats::CreateDialogWithKeysRequest request;
    request.deserialize(&serializer, dialogData);

    quint64 sender_id = request.senderId();
    quint64 receiver_id = request.receiverId();
    QByteArray sender_encrypted_session_key = request.senderEncryptedSessionKey();
    QByteArray receiver_encrypted_session_key = request.receiverEncryptedSessionKey();
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user1"] = sender_id;
    params[":user2"] = receiver_id;

    databaseConnector->executeQuery(query, "SELECT dialog_id FROM dialogs WHERE (user1_id = :user1 AND user2_id = :user2) OR (user1_id = :user2 AND user2_id = :user1)",params);
    if (query.next()) {
        return QByteArray();
    } else {
        QMap<QString, QVariant> insertParams;
        insertParams[":user1"] = sender_id;
        insertParams[":user2"] = receiver_id;
        insertParams[":sender_encrypted_session_key"] = sender_encrypted_session_key;
        insertParams[":receiver_encrypted_session_key"] = receiver_encrypted_session_key;
        databaseConnector->executeQuery(query,"INSERT INTO dialogs (user1_id, user2_id, encrypted_session_key_user1, encrypted_session_key_user2) VALUES (:user1, :user2, :sender_encrypted_session_key, :receiver_encrypted_session_key)",insertParams);

        quint64 dialog_id = query.lastInsertId().toULongLong();

        chats::CreateDialogWithKeysResponse response;
        response.setUniqMessageId(request.uniqMessageId());
        response.setReceiverId(request.receiverId());
        response.setReceiverEncryptedSessionKey(getEncryptedSessionKey(dialog_id, receiver_id));
        response.setSenderEncryptedSessionKey(getEncryptedSessionKey(dialog_id, sender_id));

        return response.serialize(&serializer);
    }
}

quint64 ChatManager::getDialog(const quint64 &sender_id, const quint64 &receiver_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user1"] = sender_id;
    params[":user2"] = receiver_id;
    databaseConnector->executeQuery(query, "SELECT dialog_id FROM dialogs WHERE (user1_id = :user1 AND user2_id = :user2) OR (user1_id = :user2 AND user2_id = :user1)",params);
    if (query.next()) {
        return query.value(0).toULongLong();
    } else {
        return 0;
    }
}

QByteArray ChatManager::getEncryptedSessionKey(const quint64 &dialog_id, const quint64 &user_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":dialog_id"] = dialog_id;
    params[":user_id"] = user_id;

    QString sqlQuery =
        "SELECT CASE "
        "WHEN user1_id = :user_id THEN encrypted_session_key_user1 "
        "WHEN user2_id = :user_id THEN encrypted_session_key_user2 "
        "END AS encrypted_session_key "
        "FROM dialogs WHERE dialog_id = :dialog_id";

    databaseConnector->executeQuery(query, sqlQuery, params);

    if (query.next()) {
        return query.value(0).toByteArray();
    } else return QByteArray();

    return QByteArray();
}

QByteArray ChatManager::getDataForCreateDialog(const QByteArray &requestData)
{
    QProtobufSerializer serializer;
    chats::CreateDialogRequest request;

    if (!request.deserialize(&serializer, requestData)) {
        logger.log(Logger::INFO, "chatmanager.cpp::getDataForCreateDialog", "Error deserialize request");
        return QByteArray();
    }

    chats::CreateDialogResponse response;
    response.setSenderPublicKey(databaseConnector->getUserManager()->getUserPublicKey(request.senderId()));
    response.setReceiverPublicKey(databaseConnector->getUserManager()->getUserPublicKey(request.receiverId()));
    response.setUniqMessageId(request.uniqMessageId());
    response.setReceiverId(request.receiverId());

    return response.serialize(&serializer);
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

quint64 ChatManager::saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &special_type, const QString &flag)
{
    QSqlQuery query;
    QString queryStr;
    QMap<QString, QVariant> params;
    if(flag == "personal") {
        params[":receiver_id"] = receiverId;
        params[":dialog_id"] = dialogId;
        queryStr = "INSERT INTO messages (sender_id, receiver_id, dialog_id, content, media_url, special_type) VALUES (:sender_id, :receiver_id, :dialog_id, :content, :fileUrl, :special_type)";
    } else if (flag == "group") {
        params[":group_id"] = receiverId;
        queryStr = "INSERT INTO messages (sender_id, group_id, content, media_url, special_type) VALUES (:sender_id, :group_id, :content, :fileUrl, :special_type)";
    }
    params[":sender_id"] = senderId;
    params[":content"] = message;
    params[":fileUrl"] = fileUrl;
    params[":special_type"] = special_type;
    if (!databaseConnector->executeQuery(query,queryStr,params)) {
        logger.log(Logger::INFO,"chatmanager.cpp::saveMessage", "Error exec query: " + query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toInt();
}

QByteArray ChatManager::loadMessages(const QByteArray &requestData)
{
    QProtobufSerializer serializer;
    chats::LoadMessagesRequest request;

    if (!request.deserialize(&serializer, requestData)) {
        logger.log(Logger::INFO, "chatmanager.cpp::loadMessages", "Error deserialize request");
        return QByteArray();
    }

    quint64 chat_id = request.chatId();
    quint64 user_id = request.userId();
    quint32 offset = request.offset();
    QString chat_name;
    QString queryStr;
    QMap<QString, QVariant> params;
    params[":offset"] = offset;

    int dialog_id;
    if (request.type() == chats::ChatTypeGadget::ChatType::PERSONAL) {
        chat_name = databaseConnector->getUserManager()->getUserLogin(chat_id);
        dialog_id = getDialog(user_id, chat_id);
        if(dialog_id == 0) {
            return QByteArray();
        }

        params[":dialog_id"] = dialog_id;
        queryStr = "SELECT message_id, content, media_url, timestamp, sender_id, receiver_id, special_type FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset";
    } else if(request.type() == chats::ChatTypeGadget::ChatType::GROUP) {
        chat_name = databaseConnector->getGroupManager()->getGroupName(chat_id);
        params[":group_id"] = chat_id;
        queryStr = "SELECT message_id, content, media_url, timestamp, sender_id, special_type FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset";
    }

    QSqlQuery query;
    chats::LoadMessagesResponse response;
    QList<chats::ChatMessage> messages;

    if (!databaseConnector->executeQuery(query,queryStr,params)){
        logger.log(Logger::INFO,"chatmanager.cpp::loadMessagesProcess", "Error exec query: " + query.lastError().text());
        return QByteArray();
    }

    while (query.next()) {
        if (request.type() == chats::ChatTypeGadget::ChatType::PERSONAL) {
            chats::ChatMessage message = generatePersonalMessageObject(query);
            message.setSenderEncryptedSessionKey(getEncryptedSessionKey(dialog_id,message.senderId()));
            message.setReceiverEncryptedSessionKey(getEncryptedSessionKey(dialog_id,message.receiverId()));
            messages.prepend(message);
        } else if (request.type() == chats::ChatTypeGadget::ChatType::GROUP) {
            chats::ChatMessage message = generateGroupMessageObject(query);
            message.setGroupId(chat_id);
            message.setIsRead(getGroupMessageReadStatus(message.messageId(), user_id, message.senderId()));
            message.setGroupName(databaseConnector->getGroupManager()->getGroupName(chat_id));
            messages.prepend(message);
        }
    }

    response.setMessages(messages);
    response.setType(request.type());
    response.setChatName(chat_name);

    return response.serialize(&serializer);
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

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id, receiver_id, special_type FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50", params)) {
            logger.log(Logger::INFO,"chatmanager.cpp::getUserMessages", "Error exec query: " + query.lastError().text());
            continue;
        }
        while (query.next()) {
            chats::ChatMessage message = generatePersonalMessageObject(query);
            message.setSenderEncryptedSessionKey(getEncryptedSessionKey(dialog_id,message.senderId()));
            message.setReceiverEncryptedSessionKey(getEncryptedSessionKey(dialog_id,message.receiverId()));
            messages.prepend(message);
        }
    }
    QList<int> groupIds = databaseConnector->getGroupManager()->getUserGroups(user_id);
    for (int group_id : groupIds) {
        QSqlQuery query;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;

        if (!databaseConnector->executeQuery(query, "SELECT message_id, content, media_url, timestamp, sender_id, special_type FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50", params)) {
            logger.log(Logger::INFO,"chatmanager.cpp::getUserMessages", "Error exec query: " + query.lastError().text());
            continue;
        }

        while (query.next()) {
            chats::ChatMessage message = generateGroupMessageObject(query);
            message.setGroupId(group_id);
            message.setIsRead(getGroupMessageReadStatus(message.messageId(), user_id, message.senderId()));
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
        logger.log(Logger::INFO,"chatmanager.cpp::getUserDialogs", "Error getting dialogs for user_id: " + QString::number(user_id) + " with error: " + query.lastError().text());
    }
    return dialogIds;
}

bool ChatManager::getMessageReadStatus(const quint64 &message_id)
{
    QMap<QString, QVariant> params;
    params[":message_id"] = message_id;

    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,
                                         "SELECT COUNT(*) FROM message_reads WHERE message_id = :message_id",
                                         params)) {
        return false;
    }
    if (query.next()) {
        return (query.value(0).toInt() > 0);
    }
    return false;
}

bool ChatManager::getGroupMessageReadStatus(const quint64 &message_id, const quint64 &user_id, const quint64 &sender_id)
{
    QMap<QString, QVariant> params;
    params[":message_id"] = message_id;
    QSqlQuery query;

    if(user_id == sender_id) {
        return getMessageReadStatus(message_id);
    } else {
        params[":user_id"] = user_id;
        if (!databaseConnector->executeQuery(query,
                                             "SELECT COUNT(*) FROM message_reads WHERE message_id = :message_id AND user_id = :user_id",
                                             params)) {
            return false;
        }
        if(query.next()) {
            return (query.value(0).toInt() > 0);
        }
        return false;
    }
}

chats::ChatMessage ChatManager::generatePersonalMessageObject(QSqlQuery &query)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();
    int receiverId = query.value(5).toInt();
    QString special_type = query.value(6).toString();

    if (message_id + senderId + receiverId == 0) return chats::ChatMessage();

    chats::ChatMessage message;
    message.setMessageId(message_id);
    message.setIsRead(getMessageReadStatus(message_id));
    message.setMediaUrl(fileUrl);
    message.setTimestamp(timestamp);
    message.setReceiverId(receiverId);
    message.setSenderId(senderId);
    message.setSenderLogin(databaseConnector->getUserManager()->getUserLogin(senderId));
    message.setReceiverLogin(databaseConnector->getUserManager()->getUserLogin(receiverId));
    message.setContent(content);
    message.setSpecialType(special_type);

    return message;
}

chats::ChatMessage ChatManager::generateGroupMessageObject(QSqlQuery &query)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();
    QString special_type = query.value(5).toString();

    chats::ChatMessage message;
    message.setMessageId(message_id);
    message.setMediaUrl(fileUrl);
    message.setTimestamp(timestamp);
    message.setSenderId(senderId);
    message.setSenderLogin(databaseConnector->getUserManager()->getUserLogin(senderId));
    message.setContent(content);
    message.setSpecialType(special_type);

    return message;
}
