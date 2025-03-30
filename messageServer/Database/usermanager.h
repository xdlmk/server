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

#include "../../Utils/logger.h"

class DatabaseConnector;
class ChatNetworkManager;

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QJsonObject loginUser(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket);
    QJsonObject registerUser(const QJsonObject &json);
    QJsonObject searchUsers(const QJsonObject &json);
    QJsonObject editUserProfile(const QJsonObject &dataEditProfile);
    QJsonObject getCurrentAvatarUrlById(const QJsonObject &avatarsUpdate);

    int getUserId(const QString &userlogin);
    QString getUserLogin(int user_id);

    void setUserAvatar(const QString &avatarUrl, int user_id);
    QString getUserAvatar(int user_id);
private:
    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // USERMANAGER_H
