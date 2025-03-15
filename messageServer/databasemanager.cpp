#include "databasemanager.h"
#include "chatnetworkmanager.h"

DatabaseManager& DatabaseManager::instance(QObject *parent) {
    static DatabaseManager instance(parent);
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject{parent}
{}

void DatabaseManager::connectToDB() {

    QSqlDatabase db = QSqlDatabase::addDatabase("QMARIADB");
    db.setHostName("localhost");
    db.setPort(3306);
    db.setDatabaseName("test_db");
    db.setUserName("admin");
    db.setPassword("admin-password");

    if(db.open()) qDebug() << "Database is connected";
    else {
        qDebug() << "Available drivers:" << QSqlDatabase::drivers();
        QSqlError error = db.lastError();
        qDebug() << "Error connecting to database:" << error.text();
    }
}

QJsonObject DatabaseManager::loginProcess(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket) {
    QString login = json["login"].toString();
    QString password = json["password"].toString();
    QJsonObject jsonLogin;
    jsonLogin["flag"] = "login";
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Error executing query during password_hash:" << query.lastError().text();
        jsonLogin["success"] = "error";
    }
    else {
        query.next();
        QString passwordHash = query.value(0).toString();
        if (checkPassword(password,passwordHash))
        {
            jsonLogin["success"] = "ok";
            jsonLogin["name"] = json["login"];
            jsonLogin["password"] = json["password"];

            query.prepare("SELECT avatar_url, id_user FROM users WHERE userlogin = :userlogin");
            query.bindValue(":userlogin", login);
            int id;
            QString avatar_url;
            if (query.exec()) {
                if (query.next()) {
                    avatar_url = query.value(0).toString();
                    id = query.value(1).toInt();

                    jsonLogin["avatar_url"] = avatar_url;
                    jsonLogin["user_id"] = id;
                } else {
                    qDebug() << "No user found with login:" << login;
                }
            } else {
                qDebug() << "Error executing query during login:" << query.lastError().text();
            }

            if(manager) {
                manager->setIdentifiersForClient(socket,login,id);
            }
        } else {
            jsonLogin["success"] = "poor";
        }
    }
    return jsonLogin;
}

QJsonObject DatabaseManager::regProcess(QJsonObject json) {
    QString login = json["login"].toString();
    QString password = hashPassword(json["password"].toString());
    QJsonObject jsonReg;
    jsonReg["flag"] = "reg";

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Error executing query during registration(select count) :" << query.lastError().text();
        jsonReg["success"] = "error";
    }
    else {
        query.next();
        int count = query.value(0).toInt();
        if (count > 0) {
            qDebug() << "Exec = poor";
            jsonReg["success"] = "poor";
            jsonReg["errorMes"] = "This username is taken";

        } else {
            qDebug() << "Exec = ok";
            jsonReg["success"] = "ok";

            query.prepare("INSERT INTO `users` (`username`, `password_hash`, `userlogin`) VALUES (:username, :password_hash, :username);");
            query.bindValue(":username", login);
            query.bindValue(":password_hash",password);
            jsonReg["name"] = json["login"];
            jsonReg["password"] = json["password"];
            if (!query.exec()) {
                qDebug() << "Error executing query during registration(insert):" << query.lastError().text();
                jsonReg["success"] = "error";
                jsonReg["errorMes"] = "Error with insert to database";
            }
        }
    }
    return jsonReg;
}

QJsonObject DatabaseManager::searchProcess(QJsonObject json) {
    QString searchable = json["searchable"].toString();

    QJsonArray jsonArray;
    QSqlQuery query;
    query.prepare("SELECT id_user, userlogin, avatar_url FROM users WHERE userlogin LIKE :keyword");
    query.bindValue(":keyword", "%" + searchable + "%");
    if (query.exec()) {
        while (query.next()) {

            int id = query.value(0).toInt();
            QString userlogin = query.value(1).toString();
            QString avatar_url = query.value(2).toString();

            QJsonObject userObject;
            userObject["id"] = id;
            userObject["userlogin"] = userlogin;
            userObject["avatar_url"] = avatar_url;

            jsonArray.append(userObject);
        }
    } else {
        qDebug() << "Error executing query during search: " << query.lastError().text();
    }

    QJsonObject searchJson;
    searchJson["flag"] = "search";
    searchJson["results"] = jsonArray;
    return searchJson;
}

QJsonObject DatabaseManager::editProfileProcess(QJsonObject dataEditProfile) {
    QString editable = dataEditProfile["editable"].toString();
    QString editInformation = dataEditProfile["editInformation"].toString();
    int user_id = dataEditProfile["user_id"].toInt();
    QSqlQuery query;
    QString bindingValue;
    if(editable == "Name") bindingValue = "username";
    else if(editable == "Phone number") bindingValue = "phone_number";
    else if(editable == "Username") bindingValue = "userlogin";

    query.prepare("UPDATE users SET " + bindingValue + " = :" + bindingValue + " WHERE id_user = :id_user");
    query.bindValue(":" + bindingValue, editInformation);
    query.bindValue(":id_user", user_id);

    QJsonObject editResults;
    editResults["flag"] = "edit";

    if (!query.exec()) {
        qDebug() << "Query execution error:" << query.lastError().text();
        qDebug() << "Query execution error code:" << query.lastError().nativeErrorCode();

        if (query.lastError().text().contains("Duplicate entry") or query.lastError().nativeErrorCode() == "1062") {
            qDebug() << "Error: Violation of uniqueness";
            editResults["status"] = "poor";
            editResults["error"] = "Unique error";
            return editResults;
        }
        editResults["status"] = "poor";
        editResults["error"] = "Unknown error";
        return editResults;
    }
    editResults["status"] = "ok";
    editResults["editable"] = editable;
    editResults["editInformation"] =  editInformation;

    return editResults;
}

QJsonObject DatabaseManager::getCurrentAvatarUrlById(const QJsonObject &avatarsUpdate) {
    QJsonArray idsArray = avatarsUpdate["ids"].toArray();
    QJsonArray groupIds = avatarsUpdate["groups_ids"].toArray();
    QJsonObject avatarsUpdateJson;
    avatarsUpdateJson["flag"] = "avatars_update";

    QJsonArray avatarsArray;
    QJsonArray groupsAvatarsArray;

    for (const QJsonValue &value : idsArray) {
        int id = value.toInt();
        QString avatarUrl = getAvatarUrl(id);
        QJsonObject avatarObject;
        avatarObject["id"] = id;
        avatarObject["avatar_url"] = avatarUrl;
        avatarsArray.append(avatarObject);
    }

    for (const QJsonValue &value : groupIds) {
        int id = value.toInt();
        QString avatarUrl = getGroupAvatarUrl(id);
        QJsonObject avatarObject;
        avatarObject["group_id"] = id;
        avatarObject["avatar_url"] = avatarUrl;
        groupsAvatarsArray.append(avatarObject);
    }
    avatarsUpdateJson["avatars"] = avatarsArray;
    avatarsUpdateJson["groups_avatars"] = groupsAvatarsArray;
    return avatarsUpdateJson;
}

QJsonObject DatabaseManager::updatingChatsProcess(QJsonObject json, ChatNetworkManager *manager) {
    QJsonArray jsonMessageArray;
    QJsonObject updatingJson;
    updatingJson["flag"] = "updating_chats";
    getUserMessages(json, jsonMessageArray, manager);

    updatingJson["messages"] = jsonMessageArray;
    return updatingJson;
}

QJsonObject DatabaseManager::loadMessagesProcess(QJsonObject requestJson)
{
    int chat_id = requestJson["chat_id"].toInt();
    int user_id = requestJson["user_id"].toInt();
    int offset = requestJson["offset"].toInt();
    QString type = requestJson["type"].toString();

    int dialog_id = getOrCreateDialog(user_id,chat_id);

    QJsonArray jsonMessageArray;

    QSqlQuery query;
    if(type == "personal") {
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset");
        query.bindValue(":dialog_id", dialog_id);
        query.bindValue(":offset", offset);
    } else if(type == "group") {
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50 OFFSET :offset");
        query.bindValue(":group_id", chat_id);
        query.bindValue(":offset", offset);
    }

    if (!query.exec()) qDebug() << "Fail execute query (loadMessagesProcess): " << query.lastError().text();
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

            QSqlQuery queryName;
            queryName.prepare("SELECT group_name FROM group_chats WHERE group_id = :group_id");
            queryName.bindValue(":group_id", chat_id);
            if (queryName.exec() && queryName.next()) {
                messageObject["group_name"] = queryName.value(0).toString();
            }

            messageObject["sender_id"] = senderId;
            messageObject["sender_login"] = getUserLogin(senderId);
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

QJsonObject DatabaseManager::getGroupInformation(QJsonObject json)
{
    QString userlogin = json["userlogin"].toString();
    int user_id = getUserId(userlogin);
    QList<int> groupIds = getUserGroups(user_id);
    QJsonObject groupsInfo;
    groupsInfo["flag"] = "group_info";

    QJsonArray groupsInfoArray;
    for (int group_id : groupIds) {
        QJsonObject groupInfoJson;

        int creator_id;
        QSqlQuery queryName;
        queryName.prepare("SELECT group_name, avatar_url, created_by FROM group_chats WHERE group_id = :group_id");
        queryName.bindValue(":group_id", group_id);
        if(queryName.exec() && queryName.next());
        groupInfoJson["group_name"] = queryName.value(0).toString();
        groupInfoJson["avatar_url"] = queryName.value(1).toString();
        creator_id = queryName.value(2).toInt();
        groupInfoJson["group_id"] = group_id;

        QJsonArray groupMembersArray;

        QSqlQuery queryMembers;
        queryMembers.prepare("SELECT user_id FROM group_members WHERE group_id = :group_id");
        queryMembers.bindValue(":group_id", group_id);
        if (queryMembers.exec()) {
            while (queryMembers.next()) {
                QJsonObject member;
                int user_id = queryMembers.value(0).toInt();
                member["id"] = user_id;
                member["username"] = getUserLogin(user_id);
                member["status"] = user_id == creator_id ? "creator" : "member";
                member["avatar_url"] = getAvatarUrl(user_id);

                groupMembersArray.append(member);
            }
        }
        groupInfoJson["members"] = groupMembersArray;
        groupsInfoArray.append(groupInfoJson);
    }
    groupsInfo["info"] = groupsInfoArray;
    return groupsInfo;
}

QJsonObject DatabaseManager::getDialogsInformation(QJsonObject json)
{
    QString userlogin = json["userlogin"].toString();
    int user_id = getUserId(userlogin);
    QList<int> dialogsIds = getUserDialogs(user_id);
    QJsonObject dialogsInfo;
    dialogsInfo["flag"] = "dialogs_info";

    QJsonArray dialogsInfoArray;
    for (int dialog_id : dialogsIds) {
        QJsonObject dialogsInfoJson;
        if(dialog_id == 0) continue;

        QSqlQuery query;
        query.prepare("SELECT CASE WHEN user1_id = :user_id THEN user2_id ELSE user1_id END AS second_user_id "
                      "FROM dialogs WHERE dialog_id = :dialog_id");
        query.bindValue(":dialog_id", dialog_id);
        query.bindValue(":user_id", user_id);

        if (query.exec() && query.next()) {
            int second_user_id = query.value(0).toInt();

            QSqlQuery userQuery;
            userQuery.prepare("SELECT * FROM users WHERE id_user = :second_user_id");
            userQuery.bindValue(":second_user_id", second_user_id);

            if (userQuery.exec() && userQuery.next()) {
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

QJsonObject DatabaseManager::deleteMemberFromGroup(const QJsonObject deleteMemberJson)
{
    int user_id_deletion = deleteMemberJson["user_id"].toInt();
    int group_id = deleteMemberJson["group_id"].toInt();
    int creator_id = deleteMemberJson["creator_id"].toInt();
    QJsonObject deleteMemberResult;
    deleteMemberResult["flag"] = "delete_member";
    deleteMemberResult["group_id"] = group_id;
    deleteMemberResult["sender_id"] = creator_id;

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM group_chats WHERE group_id = :group_id AND created_by = :created_by");
    query.bindValue(":group_id", group_id);
    query.bindValue(":created_by", creator_id);
    query.exec();
    query.next();

    int count = query.value(0).toInt();
    if (count > 0) {
        query.prepare("DELETE FROM group_members WHERE user_id = :user_id AND group_id = :group_id");
        query.bindValue(":user_id", user_id_deletion);
        query.bindValue(":group_id", group_id);

        query.exec();

        if (query.numRowsAffected() > 0) {
            qDebug() << "Member delete success id: " << user_id_deletion << " for group_id =" << group_id;
            deleteMemberResult["deleted_user_id"] = user_id_deletion;
            deleteMemberResult["error_code"] = 0;
        } else {
            qDebug() << "User with user_id: " << user_id_deletion << "not a member group with group_id: " << group_id;
            deleteMemberResult["error_code"] = 1;
        }
    } else {
        qDebug() << "User with id: " << creator_id << " not the admin of the group with id: " << group_id;
        deleteMemberResult["error_code"] = 2;
    }
    return deleteMemberResult;
}

QJsonObject DatabaseManager::addMemberToGroup(const QJsonObject addMemberJson)
{
    int user_id = addMemberJson["id"].toInt();
    int group_id = addMemberJson["group_id"].toInt();
    int admin_id = addMemberJson["admin_id"].toInt();
    QJsonObject addMemberResult;
    addMemberResult["flag"] = "add_group_members";
    addMemberResult["group_id"] = group_id;
    addMemberResult["sender_id"] = admin_id;

    QJsonArray newMembers = addMemberJson["members"].toArray();

    QList<int> idsMembers;
    QJsonArray addedMembers;
    for (const QJsonValue &value : newMembers) {
        QJsonObject memberObject = value.toObject();
        int id = memberObject["id"].toInt();
        idsMembers.append(id);
    }
    for(int id : idsMembers) {
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM group_members WHERE group_id = :group_id AND user_id = :user_id");
        checkQuery.bindValue(":group_id", group_id);
        checkQuery.bindValue(":user_id", id);
        if (!checkQuery.exec() || !checkQuery.next()) {
            qWarning() << "Failed exec check query: " << checkQuery.lastError().text();
            continue;
        }

        int count = checkQuery.value(0).toInt();
        if (count == 0) {
            QSqlQuery query;
            query.prepare("INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)");
            query.bindValue(":group_id", group_id);
            query.bindValue(":user_id", id);

            if(!query.exec()) {
                qWarning() << "Failed to insert into group_members(add new Member):" << query.lastError().text();
            } else {
                QJsonObject member;
                member["avatar_url"] = getAvatarUrl(id);
                member["id"] = id;
                member["status"] = "member";
                member["username"] = getUserLogin(id);
                addedMembers.append(member);
            }
        }
    }
    addMemberResult["addedMembers"] = addedMembers;
    return addMemberResult;
}

void DatabaseManager::saveFileToDatabase(const QString &fileUrl)
{
    QSqlQuery query;
    query.prepare("INSERT INTO `files` (`file_url`) VALUES (:fileUrl);");
    query.bindValue(":fileUrl", fileUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query:" << query.lastError().text();
    }
}

void DatabaseManager::setAvatarInDatabase(const QString &avatarUrl, const int &user_id)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET avatar_url = :avatar_url WHERE id_user = :id_user;");
    query.bindValue(":id_user", user_id);
    query.bindValue(":avatar_url", avatarUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query to setAvatarInDatabase:" << query.lastError().text();
    }
}

void DatabaseManager::setGroupAvatarInDatabase(const QString &avatarUrl, const int &group_id)
{
    qDebug() << "setGroupAvatarInDatabase starts";
    QSqlQuery query;
    query.prepare("UPDATE group_chats SET avatar_url = :avatar_url WHERE group_id = :group_id;");
    query.bindValue(":group_id", group_id);
    query.bindValue(":avatar_url", avatarUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query to setGroupAvatarInDatabase:" << query.lastError().text();
    }
}

QString DatabaseManager::getAvatarUrl(const int &user_id) {
    qDebug() << "getAvatarUrl starts with id " + user_id;
    QSqlQuery query;
    query.prepare("SELECT avatar_url FROM users WHERE id_user = :user_id");
    query.bindValue(":user_id", user_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "query getAvatarUrl error: " << query.lastError().text();
    }
    return "";
}

QString DatabaseManager::getGroupAvatarUrl(const int &group_id)
{
    qDebug() << "getGroupAvatarUrl starts with id " + group_id;
    QSqlQuery query;
    query.prepare("SELECT avatar_url FROM group_chats WHERE group_id = :group_id");
    query.bindValue(":group_id", group_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "query getGroupAvatarUrl error: " << query.lastError().text() << "for id: " << group_id;
    }
    return "";
}

QList<int> DatabaseManager::getGroupMembers(const int &group_id)
{
    QList<int> members;
    QSqlQuery query;

    query.prepare("SELECT user_id FROM group_members WHERE group_id = :group_id");
    query.bindValue(":group_id", group_id);
    if (query.exec()) {
        while (query.next()) {
            members.append(query.value(0).toInt());
        }
    } else {
        qDebug() << "Database error: " << query.lastError().text();
    }
    return members;
}

void DatabaseManager::createGroup(QJsonObject json, ChatNetworkManager *manager)
{
    QSqlQuery createGroupQuery;
    createGroupQuery.prepare("INSERT INTO group_chats (group_name, created_by, avatar_url) VALUES (:group_name, :created_by, :avatar_url)");
    createGroupQuery.bindValue(":group_name", json["groupName"].toString());
    createGroupQuery.bindValue(":created_by", json["creator_id"].toInt());
    createGroupQuery.bindValue(":avatar_url", json["avatar_url"].toString());

    if (!createGroupQuery.exec()) {
        qWarning() << "Failed to insert into group_chats:" << createGroupQuery.lastError().text();
    }
    int groupId = createGroupQuery.lastInsertId().toInt();

    QSqlQuery insertMemberQuery;

    QJsonArray membersArray = json["members"].toArray();

    QList<int> idsMembers;
    for (const QJsonValue &value : membersArray) {
        QJsonObject memberObject = value.toObject();
        int id = memberObject["id"].toInt();
        idsMembers.append(id);
    }
    idsMembers.append(json["creator_id"].toInt());
    for(int id : idsMembers) {
        insertMemberQuery.prepare("INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)");
        insertMemberQuery.bindValue(":group_id", groupId);
        insertMemberQuery.bindValue(":user_id", id);
        if (!insertMemberQuery.exec()) {
            qWarning() << "Failed to insert into group_members:" << insertMemberQuery.lastError().text();
        }
    }
    QJsonObject groupCreateJson;
    groupCreateJson["flag"] = "group_message";
    groupCreateJson["message"] = "Created this group";

    groupCreateJson["sender_login"] = getUserLogin(json["creator_id"].toInt());
    groupCreateJson["sender_id"] = json["creator_id"].toInt();
    groupCreateJson["group_name"] = json["groupName"].toString();
    groupCreateJson["group_id"] = groupId;
    groupCreateJson["group_avatar_url"] = json["avatar_url"].toString();
    groupCreateJson["only_create"] = "true";

    MessageProcessor::groupMessageProcess(groupCreateJson,manager);
}

int DatabaseManager::getOrCreateDialog(int sender_id, int receiver_id)
{
    QSqlQuery query;

    query.prepare("SELECT dialog_id FROM dialogs WHERE (user1_id = :user1 AND user2_id = :user2) OR (user1_id = :user2 AND user2_id = :user1)");
    query.bindValue(":user1", sender_id);
    query.bindValue(":user2", receiver_id);
    query.exec();

    if (query.next()) {
        return query.value(0).toInt();
    } else {
        query.prepare("INSERT INTO dialogs (user1_id, user2_id) VALUES (:user1, :user2)");
        query.bindValue(":user1", sender_id);
        query.bindValue(":user2", receiver_id);
        query.exec();

        return query.lastInsertId().toInt();
    }
}

int DatabaseManager::saveMessageToDatabase(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &flag)
{
    QSqlQuery query;
    if(flag == "personal") {
        query.prepare("INSERT INTO messages (sender_id, receiver_id, dialog_id, content, media_url) VALUES (:sender_id, :receiver_id, :dialog_id, :content, :fileUrl)");
        query.bindValue(":receiver_id", receiverId);
        query.bindValue(":dialog_id", dialogId);
    } else if (flag == "group") {
        query.prepare("INSERT INTO messages (sender_id, group_id, content, media_url) VALUES (:sender_id, :group_id, :content, :fileUrl)");
        query.bindValue(":group_id", receiverId);
    }
    query.bindValue(":sender_id", senderId);
    query.bindValue(":content", message);
    query.bindValue(":fileUrl", fileUrl);
    if (!query.exec()) {
        qDebug() << "Error exec query:" << query.lastError().text();
    }
    return query.lastInsertId().toInt();
}

void DatabaseManager::getUserMessages(QJsonObject json, QJsonArray &jsonMessageArray, ChatNetworkManager *manager) {
    QString userlogin = json["userlogin"].toString();
    int user_id = getUserId(userlogin);
    if (user_id == -1) return;

    QList<int> dialogIds = getUserDialogs(user_id);
    for (int dialog_id : dialogIds) {
        QSqlQuery query;
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp DESC LIMIT 50");
        query.bindValue(":dialog_id", dialog_id);

        if (!query.exec()) {
            qDebug() << "Error query execution (select message in getUserMessages):" << query.lastError().text();
            continue;
        }

        while (query.next()) {
            appendMessageObject(query, jsonMessageArray);
        }
    }
    QList<int> groupIds = getUserGroups(user_id);
    for (int group_id : groupIds) {
        QSqlQuery query;
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id FROM messages WHERE group_id = :group_id ORDER BY timestamp DESC LIMIT 50");
        query.bindValue(":group_id", group_id);
        if (!query.exec()) {
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

            QSqlQuery queryName;
            queryName.prepare("SELECT group_name FROM group_chats WHERE group_id = :group_id");
            queryName.bindValue(":group_id", group_id);
            queryName.exec();
            queryName.next();

            messageObject["group_name"] = queryName.value(0).toString();
            messageObject["sender_id"] = senderId;
            messageObject["sender_login"] = getUserLogin(senderId);
            messageObject["message_id"] = message_id;
            messageObject["str"] = content;
            messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

            jsonMessageArray.prepend(messageObject);
        }
    }
}

int DatabaseManager::getUserId(const QString &userlogin)
{
    QSqlQuery query;
    query.prepare("SELECT id_user FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", userlogin);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qDebug() << "Could not find user_id for " << userlogin;
    return -1;
}

QList<int> DatabaseManager::getUserDialogs(int user_id)
{
    QSqlQuery query;
    query.prepare("SELECT dialog_id FROM messages WHERE sender_id = :user_id OR receiver_id = :user_id");
    query.bindValue(":user_id", user_id);

    QList<int> dialogIds;
    if (query.exec()) {
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

QList<int> DatabaseManager::getUserGroups(int user_id)
{
    QSqlQuery query;
    query.prepare("SELECT group_id FROM group_members WHERE user_id = :user_id");
    query.bindValue(":user_id", user_id);

    QList<int> groupIds;
    if (query.exec()) {
        while (query.next()) {
            int groupId = query.value(0).toInt();
            if (!groupIds.contains(groupId)) {
                groupIds.append(groupId);
            }
        }
    } else {
        qDebug() << "Error getting groups for user_id: " << user_id <<" with error: "<< query.lastError().text();
    }
    return groupIds;
}

void DatabaseManager::appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray)
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
    messageObject["receiver_login"] = getUserLogin(receiverId);
    messageObject["sender_login"] = getUserLogin(senderId);
    messageObject["receiver_id"] = receiverId;
    messageObject["sender_id"] = senderId;
    messageObject["message_id"] = message_id;
    messageObject["str"] = content;
    messageObject["fileUrl"] = fileUrl;
    messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

    jsonMessageArray.prepend(messageObject);
}

QString DatabaseManager::getUserLogin(int user_id)
{
    QSqlQuery query;
    query.prepare("SELECT userlogin FROM users WHERE id_user = :user_id");
    query.bindValue(":user_id", user_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

QString DatabaseManager::hashPassword(const QString &password)
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

bool DatabaseManager::checkPassword(const QString &enteredPassword, const QString &storedHash)
{
    QString hashOfEnteredPassword = hashPassword(enteredPassword);
    return hashOfEnteredPassword == storedHash;
}
