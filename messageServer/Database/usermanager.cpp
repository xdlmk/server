#include "usermanager.h"

#include "databaseconnector.h"
#include "groupmanager.h"

#include "../chatnetworkmanager.h"

UserManager::UserManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{ }

QJsonObject UserManager::loginUser(QJsonObject json)
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
            jsonLogin["userlogin"] = json["login"];
            jsonLogin["password"] = json["password"];

            int id;
            QString avatar_url;
            if(databaseConnector->executeQuery(query, "SELECT avatar_url, id_user FROM users WHERE userlogin = :userlogin",params)) {
                if (query.next()) {
                    avatar_url = query.value(0).toString();
                    id = query.value(1).toInt();

                    jsonLogin["avatar_url"] = avatar_url;
                    jsonLogin["user_id"] = id;
                } else {
                    logger.log(Logger::DEBUG,"usermanager.cpp::loginUser", "No user found with login: " + login);
                    jsonLogin["success"] = "poor";
                }
            }
        } else jsonLogin["success"] = "poor";
    }
    return jsonLogin;
}

QByteArray UserManager::loginUser(QByteArray data)
{
    QProtobufSerializer serializer;
    auth::LoginRequest request;

    if (!request.deserialize(&serializer, data)) {
        auth::LoginResponse response;
        response.setSuccess("error");
        return response.serialize(&serializer);
    }

    QString login = request.login();
    QString password = request.password();

    auth::LoginResponse response;
    QMap<QString, QVariant> params;
    params[":userlogin"] = login;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query, "SELECT password_hash FROM users WHERE userlogin = :userlogin", params)) {
        logger.log(Logger::DEBUG, "usermanager.cpp::loginUser", "Error executing query: " + query.lastError().text());
        response.setSuccess("error");
    } else {
        query.next();
        QString passwordHash = query.value(0).toString();
        if (checkPassword(password, passwordHash)) {
            response.setSuccess("ok");
            response.setUserlogin(login);
            response.setPassword(password);

            if (databaseConnector->executeQuery(query, "SELECT avatar_url, id_user FROM users WHERE userlogin = :userlogin", params)) {
                if (query.next()) {
                    response.setAvatarUrl(query.value(0).toString());
                    response.setUserId(query.value(1).toULongLong());
                } else {
                    response.setSuccess("poor");
                }
            }
        } else {
            response.setSuccess("poor");
        }
    }

    return response.serialize(&serializer);
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
            if (!databaseConnector->executeQuery(query,"INSERT INTO `users` (`username`, `password_hash`, `userlogin`) VALUES (:username, :password_hash, :username);",insertParams)) {
                logger.log(Logger::DEBUG,"usermanager.cpp::registerUser", "Error executing INSERT query: " + query.lastError().text());
                jsonReg["success"] = "error";
                jsonReg["errorMes"] = "Error with insert to database";
            }
        }
    }
    return jsonReg;
}

QByteArray UserManager::registerUser(const QByteArray &data)
{
    QProtobufSerializer serializer;
    auth::RegisterRequest request;

    if (!request.deserialize(&serializer, data)) {
        auth::RegisterResponse response;
        response.setSuccess("error");
        response.setErrorMes("Failed to parse request");
        return response.serialize(&serializer);
    }

    QString login = request.login();
    QString password = hashPassword(request.password());

    auth::RegisterResponse response;
    QMap<QString, QVariant> params;
    params[":userlogin"] = login;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query, "SELECT COUNT(*) FROM users WHERE userlogin = :userlogin", params)) {
        logger.log(Logger::DEBUG, "usermanager.cpp::registerUser", "Error executing SELECT query: " + query.lastError().text());
        response.setSuccess("error");
        response.setErrorMes("Database error during check");
    } else {
        query.next();
        int count = query.value(0).toInt();
        if (count > 0) {
            response.setSuccess("poor");
            response.setErrorMes("This username is taken");
        } else {
            response.setSuccess("ok");

            QMap<QString, QVariant> insertParams;
            insertParams[":username"] = login;
            insertParams[":password_hash"] = password;
            if (!databaseConnector->executeQuery(query, "INSERT INTO `users` (`username`, `password_hash`, `userlogin`) VALUES (:username, :password_hash, :username);", insertParams)) {
                logger.log(Logger::DEBUG, "usermanager.cpp::registerUser", "Error executing INSERT query: " + query.lastError().text());
                response.setSuccess("error");
                response.setErrorMes("Error inserting into database");
            }
        }
    }

    return response.serialize(&serializer);
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

QByteArray UserManager::searchUsers(const QByteArray &data)
{
    QProtobufSerializer serializer;
    search::SearchRequest request;

    if (!request.deserialize(&serializer, data)) {
        search::SearchResponse response;
        return response.serialize(&serializer);
    }

    QString searchable = request.searchable();

    search::SearchResponse response;
    QMap<QString, QVariant> params;
    params[":keyword"] = "%" + searchable + "%";
    QSqlQuery query;

    QList<search::SearchResultItem> results;
    if (databaseConnector->executeQuery(query, "SELECT id_user, userlogin, avatar_url FROM users WHERE userlogin LIKE :keyword", params)) {
        while (query.next()) {
            search::SearchResultItem item;
            item.setId_proto(query.value(0).toULongLong());
            item.setUserlogin(query.value(1).toString());
            item.setAvatarUrl(query.value(2).toString());
            results.append(item);
        }
        response.setResults(results);
    } else {
        logger.log(Logger::DEBUG, "usermanager.cpp::searchUsers", "Error executing query: " + query.lastError().text());
    }

    return response.serialize(&serializer);
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

QByteArray UserManager::editUserProfile(const QByteArray &data)
{
    QProtobufSerializer serializer;
    profile::EditProfileRequest request;

    if (!request.deserialize(&serializer, data)) {
        profile::EditProfileResponse response;
        response.setStatus("error");
        response.setError("Failed to parse request");
        return response.serialize(&serializer);
    }

    int user_id = request.userId();
    QString editable = request.editable();
    QString editInformation = request.editInformation();
    QString bindingValue;

    if (editable == "Name") bindingValue = "username";
    else if (editable == "Phone number") bindingValue = "phone_number";
    else if (editable == "Username") bindingValue = "userlogin";

    QMap<QString, QVariant> params;
    params[":" + bindingValue] = editInformation;
    params[":id_user"] = user_id;
    QSqlQuery query;

    profile::EditProfileResponse response;

    if (!databaseConnector->executeQuery(query, "UPDATE users SET " + bindingValue + " = :" + bindingValue + " WHERE id_user = :id_user", params)) {
        QString errorText = query.lastError().text();
        if (errorText.contains("Duplicate entry") || query.lastError().nativeErrorCode() == "1062") {
            response.setStatus("poor");
            response.setError("Unique error");
        } else {
            response.setStatus("poor");
            response.setError("Unknown error");
        }
    } else {
        response.setStatus("ok");
        response.setEditable(editable);
        response.setEditInformation(editInformation);
    }

    return response.serialize(&serializer);
}

QJsonObject UserManager::getCurrentAvatarUrlById(const QJsonObject &avatarsUpdate)
{
    int user_id = avatarsUpdate["user_id"].toInt();
    QJsonObject avatarsUpdateJson;
    avatarsUpdateJson["flag"] = "avatars_update";

    QJsonArray avatarsArray;
    QJsonArray groupsAvatarsArray;

    QList<int> userGroups = databaseConnector->getGroupManager()->getUserGroups(user_id);
    QList<int> userDialogs = getUserInterlocutorsIds(user_id);

    for (const int &interlocutorId : userDialogs) {
        QString avatarUrl = getUserAvatar(interlocutorId);
        QJsonObject avatarObject;
        avatarObject["id"] = interlocutorId;
        avatarObject["avatar_url"] = avatarUrl;
        avatarsArray.append(avatarObject);
    }

    for (const int &group_id : userGroups) {
        QString avatarUrl = databaseConnector->getGroupManager()->getGroupAvatar(group_id);
        QJsonObject avatarObject;
        avatarObject["group_id"] = group_id;
        avatarObject["avatar_url"] = avatarUrl;
        groupsAvatarsArray.append(avatarObject);
    }
    avatarsUpdateJson["avatars"] = avatarsArray;
    avatarsUpdateJson["groups_avatars"] = groupsAvatarsArray;
    return avatarsUpdateJson;
}

QByteArray UserManager::getCurrentAvatarUrlById(const QByteArray &data)
{
    QProtobufSerializer serializer;
    avatars::AvatarsUpdateRequest request;

    if (!request.deserialize(&serializer, data)) {
        avatars::AvatarsUpdateResponse response;
        return response.serialize(&serializer);
    }

    quint64 user_id = request.userId();
    avatars::AvatarsUpdateResponse response;

    QList<int> userGroups = databaseConnector->getGroupManager()->getUserGroups(user_id);
    QList<int> userDialogs = getUserInterlocutorsIds(user_id);

    QList<avatars::AvatarItem> userAvatars;
    for (const int &interlocutorId : userDialogs) {
        QString avatarUrl = getUserAvatar(interlocutorId);
        avatars::AvatarItem avatarItem;
        avatarItem.setId_proto(interlocutorId);
        avatarItem.setAvatarUrl(avatarUrl);
        userAvatars.append(avatarItem);
    }

    QList<avatars::GroupAvatarItem> groupAvatars;
    for (const int &group_id : userGroups) {
        QString avatarUrl = databaseConnector->getGroupManager()->getGroupAvatar(group_id);
        avatars::GroupAvatarItem groupAvatarItem;
        groupAvatarItem.setGroupId(group_id);
        groupAvatarItem.setAvatarUrl(avatarUrl);
        groupAvatars.append(groupAvatarItem);
    }
    response.setAvatars(userAvatars);
    response.setGroupsAvatars(groupAvatars);

    return response.serialize(&serializer);
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
    logger.log(Logger::WARN,"usermanager.cpp::getUserLogin", "UserLogin not found: " + QString::number(user_id));
    return "";
}

QList<int> UserManager::getUserInterlocutorsIds(int user_id)
{
    QSqlQuery query;
    QMap<QString, QVariant> params;
    params[":user_id"] = user_id;

    QList<int> interlocutorsIds;
    QString queryString = "SELECT CASE "
                          "WHEN user1_id = :user_id THEN user2_id "
                          "WHEN user2_id = :user_id THEN user1_id "
                          "END AS interlocutor_id "
                          "FROM dialogs "
                          "WHERE user1_id = :user_id OR user2_id = :user_id";

    if (databaseConnector->executeQuery(query, queryString, params)) {
        while (query.next()) {
            int interlocutorId = query.value("interlocutor_id").toInt();
            if (interlocutorId != 0 && !interlocutorsIds.contains(interlocutorId)) {
                interlocutorsIds.append(interlocutorId);
            }
        }
    } else {
        logger.log(Logger::WARN, "usermanager.cpp::getUserInterlocutorsIds",
                   "Error getting interlocutors for user_id: " + QString::number(user_id) +
                       " with error: " + query.lastError().text());
    }

    return interlocutorsIds;
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
