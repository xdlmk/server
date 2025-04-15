#ifndef GROUPMANAGER_H
#define GROUPMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>

#include "usermanager.h"

#include "../../Utils/logger.h"

#include "chatsInfo.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class DatabaseConnector;
class ChatNetworkManager;

class GroupManager : public QObject
{
    Q_OBJECT
public:
    explicit GroupManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    void createGroup(const QJsonObject &json, ChatNetworkManager *manager);
    QList<messages::GroupInfoItem> getGroupInfo(const int &user_id);
    QList<int> getGroupMembers(int group_id);
    QJsonObject addMemberToGroup(const QJsonObject &addMemberJson);
    QJsonObject removeMemberFromGroup(const QJsonObject &removeMemberJson);

    void setGroupAvatar(const QString &avatarUrl, int group_id);
    QString getGroupAvatar(int group_id);
    QList<int> getUserGroups(int user_id);
    QString getGroupName(int group_id);

private:
    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // GROUPMANAGER_H
