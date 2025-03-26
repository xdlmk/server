#include "groupmanager.h"
#include "databaseconnector.h"
#include "../chatnetworkmanager.h"

GroupManager::GroupManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector)
{}

void GroupManager::createGroup(const QJsonObject &json, ChatNetworkManager *manager)
{
    QSqlQuery createGroupQuery;
    QMap<QString, QVariant> params;
    params[":group_name"] = json["groupName"].toString();
    params[":created_by"] = json["creator_id"].toInt();
    params[":avatar_url"] = json["avatar_url"].toString();

    if (!databaseConnector->executeQuery(createGroupQuery, "INSERT INTO group_chats (group_name, created_by, avatar_url) VALUES (:group_name, :created_by, :avatar_url)", params)) {
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
        QMap<QString, QVariant> memberParams;
        memberParams[":group_id"] = groupId;
        memberParams[":user_id"] = id;

        if (!databaseConnector->executeQuery(insertMemberQuery, "INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)", memberParams)) {
            qWarning() << "Failed to insert into group_members:" << insertMemberQuery.lastError().text();
        }
    }
    QJsonObject groupCreateJson;
    groupCreateJson["flag"] = "group_message";
    groupCreateJson["message"] = "created this group";

    groupCreateJson["sender_login"] = databaseConnector->getUserManager()->getUserLogin(json["creator_id"].toInt());
    groupCreateJson["sender_id"] = json["creator_id"].toInt();
    groupCreateJson["group_name"] = json["groupName"].toString();
    groupCreateJson["group_id"] = groupId;
    groupCreateJson["group_avatar_url"] = json["avatar_url"].toString();
    groupCreateJson["special_type"] = "create";

    MessageProcessor::groupMessageProcess(groupCreateJson,manager);
}

QJsonObject GroupManager::getGroupInfo(const QJsonObject &json)
{
    QString userlogin = json["userlogin"].toString();
    int user_id = databaseConnector->getUserManager()->getUserId(userlogin);
    QList<int> groupIds = getUserGroups(user_id);
    QJsonObject groupsInfo;
    groupsInfo["flag"] = "group_info";

    QJsonArray groupsInfoArray;
    for (int group_id : groupIds) {
        QJsonObject groupInfoJson;

        int creator_id;
        QSqlQuery queryName;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;
        databaseConnector->executeQuery(queryName, "SELECT group_name, avatar_url, created_by FROM group_chats WHERE group_id = :group_id", params);
        queryName.next();
        groupInfoJson["group_name"] = queryName.value(0).toString();
        groupInfoJson["avatar_url"] = queryName.value(1).toString();
        creator_id = queryName.value(2).toInt();
        groupInfoJson["group_id"] = group_id;

        QJsonArray groupMembersArray;


        for(int user_id : getGroupMembers(group_id)){
            QJsonObject member;
            member["id"] = user_id;
            member["username"] = databaseConnector->getUserManager()->getUserLogin(user_id);
            member["status"] = user_id == creator_id ? "creator" : "member";
            member["avatar_url"] = databaseConnector->getUserManager()->getUserAvatar(user_id);

            groupMembersArray.append(member);
        }

        groupInfoJson["members"] = groupMembersArray;
        groupsInfoArray.append(groupInfoJson);
    }
    groupsInfo["info"] = groupsInfoArray;
    return groupsInfo;
}

QList<int> GroupManager::getGroupMembers(int group_id)
{
    QList<int> members;
    QSqlQuery queryMembers;
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    if (databaseConnector->executeQuery(queryMembers, "SELECT user_id FROM group_members WHERE group_id = :group_id", params)) {
        while (queryMembers.next()) {
            members.append(queryMembers.value(0).toInt());
        }
    } else {
        qDebug() << "Database error: " << queryMembers.lastError().text();
    }
    return members;
}

QJsonObject GroupManager::addMemberToGroup(const QJsonObject &addMemberJson)
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
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;
        params[":user_id"] = id;

        if (!databaseConnector->executeQuery(checkQuery, "SELECT COUNT(*) FROM group_members WHERE group_id = :group_id AND user_id = :user_id",params) || !checkQuery.next()) {
            qWarning() << "Failed exec check query: " << checkQuery.lastError().text();
            continue;
        }

        int count = checkQuery.value(0).toInt();
        if (count == 0) {
            QSqlQuery query;

            if(!databaseConnector->executeQuery(query, "INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)", params)) {
                qWarning() << "Failed to insert into group_members(add new Member):" << query.lastError().text();
            } else {
                QJsonObject member;
                member["avatar_url"] = databaseConnector->getUserManager()->getUserAvatar(id);
                member["id"] = id;
                member["status"] = "member";
                member["username"] = databaseConnector->getUserManager()->getUserLogin(id);
                addedMembers.append(member);
            }
        }
    }
    addMemberResult["addedMembers"] = addedMembers;
    return addMemberResult;
}

QJsonObject GroupManager::removeMemberFromGroup(const QJsonObject &removeMemberJson)
{
    int user_id_deletion = removeMemberJson["user_id"].toInt();
    int group_id = removeMemberJson["group_id"].toInt();
    int creator_id = removeMemberJson["creator_id"].toInt();
    QJsonObject removeMemberResult;
    removeMemberResult["flag"] = "delete_member";
    removeMemberResult["group_id"] = group_id;
    removeMemberResult["sender_id"] = creator_id;

    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    params[":created_by"] = creator_id;
    databaseConnector->executeQuery(query, "SELECT COUNT(*) FROM group_chats WHERE group_id = :group_id AND created_by = :created_by",params);
    query.next();

    int count = query.value(0).toInt();
    if (count > 0) {
        QMap<QString, QVariant> deleteUserParams;
        deleteUserParams[":user_id"] = user_id_deletion;
        deleteUserParams[":group_id"] = group_id;
        databaseConnector->executeQuery(query, "DELETE FROM group_members WHERE user_id = :user_id AND group_id = :group_id",deleteUserParams);

        if (query.numRowsAffected() > 0) {
            qDebug() << "Member delete success id: " << user_id_deletion << " for group_id =" << group_id;
            removeMemberResult["deleted_user_id"] = user_id_deletion;
            removeMemberResult["error_code"] = 0;
        } else {
            qDebug() << "User with user_id: " << user_id_deletion << "not a member group with group_id: " << group_id;
            removeMemberResult["error_code"] = 1;
        }
    } else {
        qDebug() << "User with id: " << creator_id << " not the admin of the group with id: " << group_id;
        removeMemberResult["error_code"] = 2;
    }
    return removeMemberResult;
}

void GroupManager::setGroupAvatar(const QString &avatarUrl, int group_id)
{
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    params[":avatar_url"] = avatarUrl;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,"UPDATE group_chats SET avatar_url = :avatar_url WHERE group_id = :group_id;",params)) {
        qDebug() << "Error execute sql query to setGroupAvatarInDatabase:" << query.lastError().text();
    }
}

QString GroupManager::getGroupAvatar(int group_id)
{
    qDebug() << "getGroupAvatarUrl starts with id " + QString::number(group_id);
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    QSqlQuery query;
    if (databaseConnector->executeQuery(query,"SELECT avatar_url FROM group_chats WHERE group_id = :group_id",params) && query.next()) {
        return query.value(0).toString();
    } else qDebug() << "query getGroupAvatarUrl error: " << query.lastError().text();
    return "";
}

QList<int> GroupManager::getUserGroups(int user_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;

    QList<int> groupIds;
    if (databaseConnector->executeQuery(query, "SELECT group_id FROM group_members WHERE user_id = :user_id", params)) {
        while (query.next()) {
            int groupId = query.value(0).toInt();
            if (!groupIds.contains(groupId)) {
                groupIds.append(groupId);
            }
        }
    } else qDebug() << "Error getting groups for user_id: " << user_id <<" with error: "<< query.lastError().text();
    return groupIds;
}

QString GroupManager::getGroupName(int group_id)
{
    QSqlQuery queryName;
    QMap<QString, QVariant> paramsName;
    paramsName[":group_id"] = group_id;

    databaseConnector->executeQuery(queryName, "SELECT group_name FROM group_chats WHERE group_id = :group_id", paramsName) && queryName.next();
    return queryName.value(0).toString();
}
