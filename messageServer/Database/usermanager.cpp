#include "usermanager.h"

#include "databaseconnector.h"
#include "groupmanager.h"

#include "../chatnetworkmanager.h"

UserManager::UserManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{ }

QJsonObject UserManager::loginUser(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket)
{
    QString login = json["login"].toString();
    QString password = json["password"].toString();
    QJsonObject jsonLogin;
    jsonLogin["flag"] = "login";
    QMap<QString, QVariant> params;
    params[":userlogin"] = login;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query, "SELECT password_hash FROM users WHERE userlogin = :userlogin",params)) {
        logger.log(Logger::DEBUG,"usermanager.cpp::loginUser", "Error executing query during password_hash: " + query.lastError().text());
        jsonLogin["success"] = "error";
    } else {
        query.next();
        QString passwordHash = query.value(0).toString();
        if (checkPassword(password,passwordHash)) {
            jsonLogin["success"] = "ok";
            jsonLogin["name"] = json["login"];
            jsonLogin["password"] = json["password"];

            int id;
            QString avatar_url;
            if(databaseConnector->executeQuery(query, "SELECT avatar_url, id_user FROM users WHERE userlogin = :userlogin",params)) {
                if (query.next()) {
                    avatar_url = query.value(0).toString();
                    id = query.value(1).toInt();

                    jsonLogin["avatar_url"] = avatar_url;
                    jsonLogin["user_id"] = id;

                    if(manager) manager->setIdentifiersForClient(socket,login,id);
                    else jsonLogin["success"] = "poor";
                } else {
                    logger.log(Logger::DEBUG,"usermanager.cpp::loginUser", "No user found with login: " + login);
                    jsonLogin["success"] = "poor";
                }
            }
        } else jsonLogin["success"] = "poor";
    }
    return jsonLogin;
}

QJsonObject UserManager::registerUser(const QJsonObject &json)
{
    QString login = json["login"].toString();
    QString password = hashPassword(json["password"].toString());
    QJsonObject jsonReg;
    jsonReg["flag"] = "reg";

    QMap<QString, QVariant> params;
    params[":userlogin"] = login;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,"SELECT COUNT(*) FROM users WHERE userlogin = :userlogin",params)) {
        logger.log(Logger::DEBUG,"usermanager.cpp::registerUser", "Error executing SELECT query: " + query.lastError().text());
        jsonReg["success"] = "error";
    } else {
        query.next();
        int count = query.value(0).toInt();
        if (count > 0) {
            jsonReg["success"] = "poor";
            jsonReg["errorMes"] = "This username is taken";

        } else {
            jsonReg["success"] = "ok";

            QMap<QString, QVariant> insertParams;
            insertParams[":username"] = login;
            insertParams[":password_hash"] = password;
            jsonReg["name"] = json["login"];
            jsonReg["password"] = json["password"];
            if (!databaseConnector->executeQuery(query,"INSERT INTO `users` (`username`, `password_hash`, `userlogin`) VALUES (:username, :password_hash, :username);",insertParams)) {
                logger.log(Logger::DEBUG,"usermanager.cpp::registerUser", "Error executing INSERT query: " + query.lastError().text());
                jsonReg["success"] = "error";
                jsonReg["errorMes"] = "Error with insert to database";
            }
        }
    }
    return jsonReg;
}

QJsonObject UserManager::searchUsers(const QJsonObject &json)
{
    QString searchable = json["searchable"].toString();

    QJsonArray jsonArray;
    QMap<QString, QVariant> params;
    params[":keyword"] = "%" + searchable + "%";
    QSqlQuery query;
    if (databaseConnector->executeQuery(query, "SELECT id_user, userlogin, avatar_url FROM users WHERE userlogin LIKE :keyword", params)) {
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
        logger.log(Logger::DEBUG,"usermanager.cpp::searchUsers", "Error executing query: " + query.lastError().text());
    }

    QJsonObject searchJson;
    searchJson["flag"] = "search";
    searchJson["results"] = jsonArray;
    return searchJson;
}

QJsonObject UserManager::editUserProfile(const QJsonObject &dataEditProfile)
{
    QString editable = dataEditProfile["editable"].toString();
    QString editInformation = dataEditProfile["editInformation"].toString();
    int user_id = dataEditProfile["user_id"].toInt();
    QString bindingValue;
    if(editable == "Name") bindingValue = "username";
    else if(editable == "Phone number") bindingValue = "phone_number";
    else if(editable == "Username") bindingValue = "userlogin";

    QMap<QString, QVariant> params;
    params[":" + bindingValue] = editInformation;
    params[":id_user"] = user_id;
    QSqlQuery query;

    QJsonObject editResults;
    editResults["flag"] = "edit";

    if (!databaseConnector->executeQuery(query,"UPDATE users SET " + bindingValue + " = :" + bindingValue + " WHERE id_user = :id_user",params)) {
        if (query.lastError().text().contains("Duplicate entry") or query.lastError().nativeErrorCode() == "1062") {
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

QJsonObject UserManager::getCurrentAvatarUrlById(const QJsonObject &avatarsUpdate)
{
    QJsonArray idsArray = avatarsUpdate["ids"].toArray();
    QJsonArray groupIds = avatarsUpdate["groups_ids"].toArray();
    QJsonObject avatarsUpdateJson;
    avatarsUpdateJson["flag"] = "avatars_update";

    QJsonArray avatarsArray;
    QJsonArray groupsAvatarsArray;

    for (const QJsonValue &value : idsArray) {
        int id = value.toInt();
        QString avatarUrl = getUserAvatar(id);
        QJsonObject avatarObject;
        avatarObject["id"] = id;
        avatarObject["avatar_url"] = avatarUrl;
        avatarsArray.append(avatarObject);
    }

    for (const QJsonValue &value : groupIds) {
        int id = value.toInt();
        QString avatarUrl = databaseConnector->getGroupManager()->getGroupAvatar(id);
        QJsonObject avatarObject;
        avatarObject["group_id"] = id;
        avatarObject["avatar_url"] = avatarUrl;
        groupsAvatarsArray.append(avatarObject);
    }
    avatarsUpdateJson["avatars"] = avatarsArray;
    avatarsUpdateJson["groups_avatars"] = groupsAvatarsArray;
    return avatarsUpdateJson;
}

int UserManager::getUserId(const QString &userlogin)
{
    QMap<QString, QVariant> params;
    params[":userlogin"] = userlogin;
    QSqlQuery query;
    if (databaseConnector->executeQuery(query,"SELECT id_user FROM users WHERE userlogin = :userlogin",params) && query.next()) {
        return query.value(0).toInt();
    }
    logger.log(Logger::DEBUG,"usermanager.cpp::getUserId", "UserId not found: " + userlogin);
    return -1;
}

bool UserManager::userIdCheck(const int user_id)
{
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,"SELECT id_user FROM users WHERE id_user = :user_id",params)) {
        logger.log(Logger::DEBUG,"usermanager.cpp::userIdCheck", "Error execute query: " + query.lastError().text());
        return false;
    }
    if (query.next()) return true;
    return false;
}

QString UserManager::getUserLogin(int user_id)
{
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;
    QSqlQuery query;
    if (databaseConnector->executeQuery(query,"SELECT userlogin FROM users WHERE id_user = :user_id",params) && query.next()) {
        return query.value(0).toString();
    }
    logger.log(Logger::INFO,"usermanager.cpp::getUserLogin", "UserLogin not found: " + QString::number(user_id));
    return "";
}

void UserManager::setUserAvatar(const QString &avatarUrl, int user_id)
{
    QMap<QString, QVariant> params;
    params[":id_user"] = user_id;
    params[":avatar_url"] = avatarUrl;
    QSqlQuery query;
    if (!databaseConnector->executeQuery(query,"UPDATE users SET avatar_url = :avatar_url WHERE id_user = :id_user;",params)) {
        logger.log(Logger::DEBUG,"usermanager.cpp::setUserAvatar", "Error execute query: " + query.lastError().text());
    }
}

QString UserManager::getUserAvatar(int user_id)
{
    logger.log(Logger::INFO,"usermanager.cpp::getUserAvatar", "Method starts with id: " + QString::number(user_id));
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;
    QSqlQuery query;
    if (databaseConnector->executeQuery(query,"SELECT avatar_url FROM users WHERE id_user = :user_id",params) && query.next()) {
        return query.value(0).toString();
    } else logger.log(Logger::DEBUG,"usermanager.cpp::getUserAvatar", "Error execute query: " + query.lastError().text());
    return "";
}

QString UserManager::hashPassword(const QString &password)
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

bool UserManager::checkPassword(const QString &enteredPassword, const QString &storedHash)
{
    QString hashOfEnteredPassword = hashPassword(enteredPassword);
    return hashOfEnteredPassword == storedHash;
}
