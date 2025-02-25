#include "databasemanager.h"
#include "chatnetworkmanager.h"

DatabaseManager& DatabaseManager::instance(QObject *parent) {
    static DatabaseManager instance(parent);
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject{parent}
{}

void DatabaseManager::connectToDB() {

    QSqlDatabase db = QSqlDatabase::addDatabase("QMARIADB");
    db.setHostName("localhost");
    db.setPort(3306);
    db.setDatabaseName("test_db");
    db.setUserName("admin");
    db.setPassword("admin-password");

    if(db.open()) qDebug() << "Database is connected";
    else {
        qDebug() << "Available drivers:" << QSqlDatabase::drivers();
        QSqlError error = db.lastError();
        qDebug() << "Error connecting to database:" << error.text();
    }
}

QJsonObject DatabaseManager::loginProcess(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket) {
    QString login = json["login"].toString();
    QString password = json["password"].toString();
    QJsonObject jsonLogin;
    jsonLogin["flag"] = "login";
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Error executing query during password_hash:" << query.lastError().text();
        jsonLogin["success"] = "error";
    }
    else {
        query.next();
        QString passwordHash = query.value(0).toString();
        if (checkPassword(password,passwordHash))
        {
            jsonLogin["success"] = "ok";
            jsonLogin["name"] = json["login"];
            jsonLogin["password"] = json["password"];

            query.prepare("SELECT avatar_url, id_user FROM users WHERE userlogin = :userlogin");
            query.bindValue(":userlogin", login);
            int id;
            QString avatar_url;
            if (query.exec()) {
                if (query.next()) {
                    avatar_url = query.value(0).toString();
                    id = query.value(1).toInt();

                    jsonLogin["avatar_url"] = avatar_url;
                    jsonLogin["user_id"] = id;
                } else {
                    qDebug() << "No user found with login:" << login;
                }
            } else {
                qDebug() << "Error executing query during login:" << query.lastError().text();
            }

            if(manager) {
                manager->setIdentifiersForClient(socket,login,id);
            }
        } else {
            jsonLogin["success"] = "poor";
        }
    }
    return jsonLogin;
}

QJsonObject DatabaseManager::regProcess(QJsonObject json) {
    QString login = json["login"].toString();
    QString password = hashPassword(json["password"].toString());
    QJsonObject jsonReg;
    jsonReg["flag"] = "reg";

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Error executing query during registration(select count) :" << query.lastError().text();
        jsonReg["success"] = "error";
    }
    else {
        query.next();
        int count = query.value(0).toInt();
        if (count > 0) {
            qDebug() << "Exec = poor";
            jsonReg["success"] = "poor";
            jsonReg["errorMes"] = "This username is taken";

        } else {
            qDebug() << "Exec = ok";
            jsonReg["success"] = "ok";

            query.prepare("INSERT INTO `users` (`username`, `password_hash`, `userlogin`) VALUES (:username, :password_hash, :username);");
            query.bindValue(":username", login);
            query.bindValue(":password_hash",password);
            jsonReg["name"] = json["login"];
            jsonReg["password"] = json["password"];
            if (!query.exec()) {
                qDebug() << "Error executing query during registration(insert):" << query.lastError().text();
                jsonReg["success"] = "error";
                jsonReg["errorMes"] = "Error with insert to database";
            }
        }
    }
    return jsonReg;
}

QJsonObject DatabaseManager::searchProcess(QJsonObject json) {
    QString searchable = json["searchable"].toString();

    QJsonArray jsonArray;
    QSqlQuery query;
    query.prepare("SELECT id_user, userlogin, avatar_url FROM users WHERE userlogin LIKE :keyword");
    query.bindValue(":keyword", "%" + searchable + "%");
    if (query.exec()) {
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
        qDebug() << "Error executing query during search: " << query.lastError().text();
    }

    QJsonObject searchJson;
    searchJson["flag"] = "search";
    searchJson["results"] = jsonArray;
    return searchJson;
}

QJsonObject DatabaseManager::editProfileProcess(QJsonObject dataEditProfile) {
    QString editable = dataEditProfile["editable"].toString();
    QString editInformation = dataEditProfile["editInformation"].toString();
    int user_id = dataEditProfile["user_id"].toInt();
    QSqlQuery query;
    QString bindingValue;
    if(editable == "Name") bindingValue = "username";
    else if(editable == "Phone number") bindingValue = "phone_number";
    else if(editable == "Username") bindingValue = "userlogin";

    query.prepare("UPDATE users SET " + bindingValue + " = :" + bindingValue + " WHERE id_user = :id_user");
    query.bindValue(":" + bindingValue, editInformation);
    query.bindValue(":id_user", user_id);

    QJsonObject editResults;
    editResults["flag"] = "edit";

    if (!query.exec()) {
        qDebug() << "Query execution error:" << query.lastError().text();
        qDebug() << "Query execution error code:" << query.lastError().nativeErrorCode();

        if (query.lastError().text().contains("Duplicate entry") or query.lastError().nativeErrorCode() == "1062") {
            qDebug() << "Error: Violation of uniqueness";
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

QJsonObject DatabaseManager::getCurrentAvatarUrlById(const QJsonArray &idsArray) {
    QJsonObject avatarsUpdateJson;
    avatarsUpdateJson["flag"] = "avatars_update";
    QJsonArray avatarsArray;

    for (const QJsonValue &value : idsArray) {
        int id = value.toInt();
        QString avatarUrl = getAvatarUrl(id);
        QJsonObject avatarObject;
        avatarObject["id"] = id;
        avatarObject["avatar_url"] = avatarUrl;
        avatarsArray.append(avatarObject);
    }
    avatarsUpdateJson["avatars"] = avatarsArray;
    return avatarsUpdateJson;
}

QJsonObject DatabaseManager::updatingChatsProcess(QJsonObject json) {
    QJsonArray jsonMessageArray;
    QJsonObject updatingJson;
    updatingJson["flag"] = "updating_chats";

    if (json.contains("chats")) {
        processChatHistory(json["chats"].toArray(), jsonMessageArray);
    } else if (json.contains("userlogin")) {
        processUserLogin(json, jsonMessageArray);
    }

    updatingJson["messages"] = jsonMessageArray;
    return updatingJson;
}

void DatabaseManager::saveFileToDatabase(const QString &fileUrl)
{
    QSqlQuery query;
    query.prepare("INSERT INTO `files` (`file_url`) VALUES (:fileUrl);");
    query.bindValue(":fileUrl", fileUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query:" << query.lastError().text();
    }
}

void DatabaseManager::setAvatarInDatabase(const QString &avatarUrl, const int &user_id)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET avatar_url = :avatar_url WHERE id_user = :id_user;");
    query.bindValue(":id_user", user_id);
    query.bindValue(":avatar_url", avatarUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query to setAvatarInDatabase:" << query.lastError().text();
    }
}

QString DatabaseManager::getAvatarUrl(const int &user_id) {
    qDebug() << "getAvatarUrl starts";
    QSqlQuery query;
    query.prepare("SELECT avatar_url FROM users WHERE id_user = :user_id");
    query.bindValue(":user_id", user_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "query getAvatarUrl error: " << query.lastError().text();
    }
    return "";
}

int DatabaseManager::getOrCreateDialog(int sender_id, int receiver_id)
{
    QSqlQuery query;

    query.prepare("SELECT dialog_id FROM dialogs WHERE (user1_id = :user1 AND user2_id = :user2) OR (user1_id = :user2 AND user2_id = :user1)");
    query.bindValue(":user1", sender_id);
    query.bindValue(":user2", receiver_id);
    query.exec();

    if (query.next()) {
        return query.value(0).toInt();
    } else {
        query.prepare("INSERT INTO dialogs (user1_id, user2_id) VALUES (:user1, :user2)");
        query.bindValue(":user1", sender_id);
        query.bindValue(":user2", receiver_id);
        query.exec();

        return query.lastInsertId().toInt();
    }
}

int DatabaseManager::saveMessageToDatabase(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl)
{
    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender_id, receiver_id, dialog_id, content, media_url) VALUES (:sender_id, :receiver_id, :dialog_id, :content, :fileUrl)");
    query.bindValue(":sender_id", senderId);
    query.bindValue(":receiver_id", receiverId);
    query.bindValue(":dialog_id", dialogId);
    query.bindValue(":content", message);
    query.bindValue(":fileUrl", fileUrl);
    if (!query.exec()) {
        qDebug() << "Error exec query:" << query.lastError().text();
    }
    return query.lastInsertId().toInt();
}

void DatabaseManager::processChatHistory(const QJsonArray &chatHistory, QJsonArray &jsonMessageArray) {
    for (const QJsonValue &value : chatHistory) {
        QJsonObject messageObject = value.toObject();
        int message_id = messageObject["message_id"].toInt();
        int dialog_id = messageObject["dialog_id"].toInt();

        if (dialog_id == 0) continue;

        QSqlQuery query;
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id AND message_id > :message_id ORDER BY timestamp");
        query.bindValue(":dialog_id", dialog_id);
        query.bindValue(":message_id", message_id);

        if (!query.exec()) {
            qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
            continue;
        }

        while (query.next()) {
            appendMessageObject(query, jsonMessageArray);
        }
    }
}

void DatabaseManager::processUserLogin(QJsonObject json, QJsonArray &jsonMessageArray) {
    QString userlogin = json["userlogin"].toString();
    QJsonArray dialogIdsArray = json["dialogIds"].toArray();
    int user_id = getUserId(userlogin);

    if (user_id == -1) return;

    QList<int> dialogIds = getUserDialogs(user_id);
    filterDialogs(dialogIds, dialogIdsArray);

    for (int dialog_id : dialogIds) {
        QSqlQuery query;
        query.prepare("SELECT message_id, content, media_url, timestamp, sender_id, receiver_id FROM messages WHERE dialog_id = :dialog_id ORDER BY timestamp");
        query.bindValue(":dialog_id", dialog_id);

        if (!query.exec()) {
            qDebug() << "Error query execution (select message in processUserLogin):" << query.lastError().text();
            continue;
        }

        while (query.next()) {
            appendMessageObject(query, jsonMessageArray);
        }
    }
}

int DatabaseManager::getUserId(const QString &userlogin)
{
    QSqlQuery query;
    query.prepare("SELECT id_user FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", userlogin);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qDebug() << "Could not find user_id for " << userlogin;
    return -1;
}

QList<int> DatabaseManager::getUserDialogs(int user_id)
{
    QSqlQuery query;
    query.prepare("SELECT dialog_id FROM messages WHERE sender_id = :user_id OR receiver_id = :user_id");
    query.bindValue(":user_id", user_id);

    QList<int> dialogIds;
    if (query.exec()) {
        while (query.next()) {
            int dialogId = query.value(0).toInt();
            if (!dialogIds.contains(dialogId)) {
                dialogIds.append(dialogId);
            }
        }
    } else {
        qDebug() << "Error getting dialogs for user_id: " << user_id <<" with error: "<< query.lastError().text();
    }
    return dialogIds;
}

void DatabaseManager::filterDialogs(QList<int> &dialogIds, const QJsonArray &dialogIdsArray)
{
    QSet<int> idsToDelete;
    for (const QJsonValue &value : dialogIdsArray) {
        idsToDelete.insert(value.toInt());
    }
    dialogIds.erase(std::remove_if(dialogIds.begin(), dialogIds.end(), [&idsToDelete](int id) {
                        return idsToDelete.contains(id);
                    }), dialogIds.end());
}

void DatabaseManager::appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray)
{
    int message_id = query.value(0).toInt();
    QString content = query.value(1).toString();
    QString fileUrl = query.value(2).toString();
    QString timestamp = query.value(3).toString();
    int senderId = query.value(4).toInt();
    int receiverId = query.value(5).toInt();

    if (message_id + senderId + receiverId == 0) return;

    QJsonObject messageObject;
    messageObject["FullDate"] = timestamp;
    messageObject["receiver_login"] = getUserLogin(receiverId);
    messageObject["sender_login"] = getUserLogin(senderId);
    messageObject["receiver_id"] = receiverId;
    messageObject["sender_id"] = senderId;
    messageObject["message_id"] = message_id;
    messageObject["str"] = content;
    messageObject["fileUrl"] = fileUrl;
    messageObject["time"] = QDateTime::fromString(timestamp, Qt::ISODate).toString("hh:mm");

    jsonMessageArray.append(messageObject);
}

QString DatabaseManager::getUserLogin(int user_id)
{
    QSqlQuery query;
    query.prepare("SELECT userlogin FROM users WHERE id_user = :user_id");
    query.bindValue(":user_id", user_id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

QString DatabaseManager::hashPassword(const QString &password)
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

bool DatabaseManager::checkPassword(const QString &enteredPassword, const QString &storedHash)
{
    QString hashOfEnteredPassword = hashPassword(enteredPassword);
    return hashOfEnteredPassword == storedHash;
}
