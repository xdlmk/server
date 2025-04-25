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

#include "generated_protobuf/login.qpb.h"
#include "generated_protobuf/register.qpb.h"
#include "generated_protobuf/search.qpb.h"
#include "generated_protobuf/editProfile.qpb.h"
#include "generated_protobuf/avatarsUpdate.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class DatabaseConnector;
class ChatNetworkManager;

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QByteArray loginUser(const QByteArray &data);
    QByteArray registerUser(const QByteArray &data);
    QByteArray searchUsers(const QByteArray &data);
    QByteArray editUserProfile(const QByteArray &data);
    QByteArray getCurrentAvatarUrlById(const QByteArray &data);

    bool userIdCheck(const int user_id);
    QString getUserLogin(int user_id);
    QList<int> getUserInterlocutorsIds(int user_id);

    void setUserAvatar(const QString &avatarUrl, int user_id);
    QString getUserAvatar(int user_id);
private:
    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // USERMANAGER_H
