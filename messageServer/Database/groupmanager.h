#ifndef GROUPMANAGER_H
#define GROUPMANAGER_H

#include <QObject>

#include "usermanager.h"

#include "../../Utils/logger.h"

#include "generated_protobuf/chatsInfo.qpb.h"
#include "generated_protobuf/createGroup.qpb.h"
#include "generated_protobuf/deleteMember.qpb.h"
#include "generated_protobuf/addMembers.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class DatabaseConnector;
class ChatNetworkManager;

class GroupManager : public QObject
{
    Q_OBJECT
public:
    explicit GroupManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    void createGroup(const QByteArray &data, ChatNetworkManager *manager);
    QList<chats::GroupInfoItem> getGroupInfo(const int &user_id);
    QList<int> getGroupMembers(int group_id);
    QByteArray addMemberToGroup(const groups::AddGroupMembersRequest &addMemberData);
    QByteArray removeMemberFromGroup(const groups::DeleteMemberRequest &request, bool &failed);

    void setGroupAvatar(const QString &avatarUrl, int group_id);
    QString getGroupAvatar(int group_id);
    QList<int> getUserGroups(int user_id);
    QString getGroupName(int group_id);

private:
    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // GROUPMANAGER_H
