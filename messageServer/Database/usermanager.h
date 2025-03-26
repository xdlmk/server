#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

#include <QCryptographicHash>

#include <QSqlQuery>
#include <QSqlError>
#include <QTcpSocket>

#include "groupmanager.h"

class DatabaseConnector;
class ChatNetworkManager;

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QJsonObject loginUser(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket);  // DatabaseManager::loginProcess
    QJsonObject registerUser(const QJsonObject &json);  // DatabaseManager::regProcess
    QJsonObject searchUsers(const QJsonObject &json);
    QJsonObject editUserProfile(const QJsonObject &dataEditProfile);  // DatabaseManager::editProfileProcess
    QJsonObject getCurrentAvatarUrlById(const QJsonObject &avatarsUpdate);// DatabaseManager::getCurrentAvatarUrlById

    int getUserId(const QString &userlogin);  // DatabaseManager::getUserId
    QString getUserLogin(int user_id);  // DatabaseManager::getUserLogin

    void setUserAvatar(const QString &avatarUrl, int user_id);  // DatabaseManager::setAvatarInDatabase
    QString getUserAvatar(int user_id);  // DatabaseManager::getAvatarUrl
private:
    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    DatabaseConnector *databaseConnector;
};

#endif // USERMANAGER_H
