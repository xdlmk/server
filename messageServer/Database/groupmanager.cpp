#include "groupmanager.h"

#include "databaseconnector.h"

#include "../messageprocessor.h"
#include "../chatnetworkmanager.h"

GroupManager::GroupManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{}

void GroupManager::createGroup(const QByteArray &data, ChatNetworkManager *manager)
{
    QProtobufSerializer serializer;
    groups::CreateGroupRequest request;
    if (!request.deserialize(&serializer, data)) {
        logger.log(Logger::DEBUG,"groupmanager.cpp::createGroup", "Failed deserialize proto");
        return;
    }

    QString groupName = request.groupName();
    quint64 creatorId = request.creatorId();
    QString avatarUrl = request.avatarUrl();

    QSqlQuery createGroupQuery;
    QMap<QString, QVariant> params;
    params[":group_name"] = groupName;
    params[":created_by"] = creatorId;
    params[":avatar_url"] = avatarUrl;

    if (!databaseConnector->executeQuery(createGroupQuery, "INSERT INTO group_chats (group_name, created_by, avatar_url) VALUES (:group_name, :created_by, :avatar_url)", params)) {
        logger.log(Logger::INFO, "groupmanager.cpp::createGroup", "Failed to insert into group_chats: " + createGroupQuery.lastError().text());
    }
    quint64 groupId = createGroupQuery.lastInsertId().toInt();

    QList<quint64> idsMembers = request.members();
    idsMembers.append(creatorId);

    QSqlQuery insertMemberQuery;
    for(quint64 id : idsMembers) {
        QMap<QString, QVariant> memberParams;
        memberParams[":group_id"] = groupId;
        memberParams[":user_id"] = id;

        if (!databaseConnector->executeQuery(insertMemberQuery, "INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)", memberParams)) {
            logger.log(Logger::INFO, "groupmanager.cpp::createGroup", "Failed to insert into group_members: " + insertMemberQuery.lastError().text());
        }
    }
    chats::ChatMessage creationMessage;
    creationMessage.setContent("created this group");
    creationMessage.setGroupId(groupId);
    creationMessage.setSenderId(creatorId);
    creationMessage.setSpecialType("create_group");

    MessageProcessor::groupMessageProcess(creationMessage.serialize(&serializer), manager);
}

QList<chats::GroupInfoItem> GroupManager::getGroupInfo(const int &user_id)
{
    QList<chats::GroupInfoItem> response;

    if (!databaseConnector->getUserManager()->userIdCheck(user_id)) {
        return response;
    }

    QList<int> groupIds = getUserGroups(user_id);

    for (int group_id : groupIds) {
        int creator_id;

        QSqlQuery queryName;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;
        if (databaseConnector->executeQuery(queryName, "SELECT group_name, avatar_url, created_by FROM group_chats WHERE group_id = :group_id", params) && queryName.next()) {
            chats::GroupInfoItem groupItem;
            groupItem.setGroupId(group_id);
            groupItem.setGroupName(queryName.value(0).toString());
            groupItem.setAvatarUrl(queryName.value(1).toString());
            creator_id = queryName.value(2).toInt();

            QList<chats::GroupMember> members;
            for (int member_id : getGroupMembers(group_id)) {
                chats::GroupMember memberItem;
                memberItem.setId_proto(member_id);
                memberItem.setUsername(databaseConnector->getUserManager()->getUserLogin(member_id));
                memberItem.setStatus(member_id == creator_id ? "creator" : "member");
                memberItem.setAvatarUrl(databaseConnector->getUserManager()->getUserAvatar(member_id));
                members.append(memberItem);
            }
            groupItem.setMembers(members);
            response.append(groupItem);
        } else {
            logger.log(Logger::DEBUG,"groupmanager.cpp::getGroupInfo", "Group with id: " + QString::number(group_id) + " not found in database");
            continue;
        }
    }
    return response;
}

QList<quint64> GroupManager::getGroupMembers(const quint64& group_id)
{
    QList<quint64> members;
    QSqlQuery queryMembers;
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;

    if (!databaseConnector->executeQuery(queryMembers,
                                         "SELECT user_id FROM group_members WHERE group_id = :group_id",
                                         params)) {
        QString errText = queryMembers.lastError().text();
        logger.log(Logger::INFO, "groupmanager.cpp::getGroupMembers", "Query exec error: " + errText);
        throw std::runtime_error("Query failed: " + errText.toStdString());
    }

    while (queryMembers.next()) {
        members.append(queryMembers.value(0).toULongLong());
    }

    return members;
}

quint64 GroupManager::getGroupIdByMessageId(const quint64 &messageId)
{
    quint64 groupId = 0;
    QMap<QString, QVariant> params;
    params[":message_id"] = messageId;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query,
                                         "SELECT group_id FROM messages WHERE message_id = :message_id",
                                         params))
        return groupId;

    if(query.next()) {
        groupId = query.value(0).toULongLong();
    } else {
        return groupId;
    }

    return groupId;
}

QByteArray GroupManager::addMemberToGroup(const groups::AddGroupMembersRequest &addMemberData)
{
    quint64 group_id = addMemberData.groupId();
    quint64 admin_id = addMemberData.adminId();

    groups::AddGroupMembersResponse response;
    response.setGroupId(group_id);
    response.setSenderId(admin_id);
    response.setTime(QDateTime::currentDateTime().toString("HH:mm"));

    QList<groups::AddedMember> members;
    for (const groups::GroupMemberContact &memberContact : addMemberData.members()) {
        int id = static_cast<int>(memberContact.userId());

        QSqlQuery checkQuery;
        QMap<QString, QVariant> params;
        params[":group_id"] = group_id;
        params[":user_id"] = id;

        if (!databaseConnector->executeQuery(checkQuery,
                                             "SELECT COUNT(*) FROM group_members WHERE group_id = :group_id AND user_id = :user_id", params) || !checkQuery.next()) {
            logger.log(Logger::DEBUG,"groupmanager.cpp::addMemberToGroup", "Failed exec check query: " + checkQuery.lastError().text());
            continue;
        }

        int count = checkQuery.value(0).toInt();
        if (count == 0) {
            QSqlQuery query;
            if (!databaseConnector->executeQuery(query,
                                                 "INSERT INTO group_members (group_id, user_id) VALUES (:group_id, :user_id)", params)) {
                logger.log(Logger::DEBUG,"groupmanager.cpp::addMemberToGroup", "Failed to insert into group_members:" + query.lastError().text());
            } else {
                groups::AddedMember addedMember;
                addedMember.setUserId(id);
                addedMember.setAvatarUrl(databaseConnector->getUserManager()->getUserAvatar(id));
                addedMember.setStatus("member");
                addedMember.setUsername(databaseConnector->getUserManager()->getUserLogin(id));
                members.append(addedMember);
            }
        }
    }
    response.setAddedMembers(members);

    QProtobufSerializer serializer;
    return response.serialize(&serializer);
}

QByteArray GroupManager::removeMemberFromGroup(const groups::DeleteMemberRequest &request, bool &failed)
{
    quint64 user_id_deletion = request.userId();
    quint64 group_id = request.groupId();
    quint64 creator_id = request.creatorId();

    groups::DeleteMemberResponse response;
    response.setGroupId(group_id);
    response.setSenderId(creator_id);
    response.setTime(QDateTime::currentDateTime().toString("HH:mm"));

    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    params[":created_by"] = creator_id;
    databaseConnector->executeQuery(query, "SELECT COUNT(*) FROM group_chats WHERE group_id = :group_id AND created_by = :created_by", params);
    query.next();

    int count = query.value(0).toInt();
    if (count > 0) {
        QMap<QString, QVariant> deleteUserParams;
        deleteUserParams[":user_id"] = user_id_deletion;
        deleteUserParams[":group_id"] = group_id;
        databaseConnector->executeQuery(query, "DELETE FROM group_members WHERE user_id = :user_id AND group_id = :group_id", deleteUserParams);

        if (query.numRowsAffected() > 0) {
            logger.log(Logger::DEBUG, "groupmanager.cpp::removeMemberFromGroup", "Member delete success id: " + QString::number(user_id_deletion) + " for group_id =" + QString::number(group_id));
            response.setDeletedUserId(user_id_deletion);
            response.setErrorCode(0);
            failed = false;
        } else {
            logger.log(Logger::DEBUG, "groupmanager.cpp::removeMemberFromGroup", "User with user_id: " + QString::number(user_id_deletion) + " not a member of group with group_id: " + QString::number(group_id));
            response.setErrorCode(1);
        }
    } else {
        logger.log(Logger::DEBUG, "groupmanager.cpp::removeMemberFromGroup", "User with id: " + QString::number(creator_id) + " is not the admin of the group with id: " + QString::number(group_id));
        response.setErrorCode(2);
    }
    QProtobufSerializer serializer;
    return response.serialize(&serializer);
}

void GroupManager::setGroupAvatar(const QString &avatarUrl, int group_id)
{
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    params[":avatar_url"] = avatarUrl;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,"UPDATE group_chats SET avatar_url = :avatar_url WHERE group_id = :group_id;",params)) {
        logger.log(Logger::INFO,"groupmanager.cpp::setGroupAvatar", "Error execute sql query: " + query.lastError().text());
    }
}

QString GroupManager::getGroupAvatar(int group_id)
{
    logger.log(Logger::DEBUG,"groupmanager.cpp::getGroupAvatar", "Method starts with id:  " + QString::number(group_id));
    QMap<QString, QVariant> params;
    params[":group_id"] = group_id;
    QSqlQuery query;
    if (databaseConnector->executeQuery(query,"SELECT avatar_url FROM group_chats WHERE group_id = :group_id",params) && query.next()) {
        return query.value(0).toString();
    } else logger.log(Logger::INFO,"groupmanager.cpp::getGroupAvatar", "Error execute sql query: " + query.lastError().text());
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
    } else logger.log(Logger::INFO,"groupmanager.cpp::getUserGroups", "Error getting groups for user_id: " + QString::number(user_id) + " with error: " + query.lastError().text());
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

void GroupManager::deleteGroup(const quint64 &group_id)
{
    QMap<QString, QVariant> params;
    params[":id_group"] = QVariant::fromValue(group_id);
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query,
                                         "SELECT COUNT(*) FROM group_chats WHERE group_id = :id_group;",
                                         params)) {

        logger.log(Logger::DEBUG, "groupmanager.cpp::deleteGroup",
                   "Query failed: " + query.lastError().text());
        throw std::runtime_error("Query failed: " + query.lastError().text().toStdString());
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        if(count <= 0) throw std::runtime_error("Group not found");
    } else throw std::runtime_error("Group not found");

    if (!databaseConnector->executeQuery(query,
                                         "DELETE FROM group_chats WHERE group_id = :id_group;",
                                         params)) {

        logger.log(Logger::DEBUG, "groupmanager.cpp::deleteGroup",
                   "Delete failed: " + query.lastError().text());
        throw std::runtime_error("Failed to delete group from database");
    }
}
