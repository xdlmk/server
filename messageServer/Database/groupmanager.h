#ifndef GROUPMANAGER_H
#define GROUPMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>

#include "../messageprocessor.h"

class DatabaseConnector;
class ChatNetworkManager;

class GroupManager : public QObject
{
    Q_OBJECT
public:
    explicit GroupManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    void createGroup(const QJsonObject &json, ChatNetworkManager *manager);  // DatabaseManager::createGroup
    QJsonObject getGroupInfo(const QJsonObject &json);  // DatabaseManager::getGroupInformation
    QList<int> getGroupMembers(int group_id);  // DatabaseManager::getGroupMembers
    QJsonObject addMemberToGroup(const QJsonObject &addMemberJson);  // DatabaseManager::addMemberToGroup
    QJsonObject removeMemberFromGroup(const QJsonObject &removeMemberJson);  // DatabaseManager::deleteMemberFromGroup

    void setGroupAvatar(const QString &avatarUrl, int group_id);  // DatabaseManager::setGroupAvatarInDatabase
    QString getGroupAvatar(int group_id);  // DatabaseManager::getGroupAvatarUrl
    QList<int> getUserGroups(int user_id);
    QString getGroupName(int group_id);

private:
    DatabaseConnector *databaseConnector;
};

#endif // GROUPMANAGER_H
