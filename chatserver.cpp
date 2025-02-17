#include "chatserver.h"

ChatServer::ChatServer(QObject *parent)
    : QTcpServer{parent}
{
    if(!this->listen(QHostAddress::Any,2020)) {
        qInfo() <<"Unable to start the server: " << this->errorString();
    } else {
        qInfo() << "Server started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
        connectToDB();
    }
}

void ChatServer::incomingConnection(qintptr handle)
{
    socket = new QTcpSocket(this);
    if(socket->setSocketDescriptor(handle)) {
        connect(socket,&QTcpSocket::readyRead,this,&ChatServer::readClient);
        connect(socket,&QTcpSocket::disconnected,this,&ChatServer::disconnectClient);
        qDebug() << "Client connect: " << handle;
    } else {
        delete socket;
    }
}

void ChatServer::saveFileToDatabase(const QString &fileUrl)
{
    QSqlQuery query;
    query.prepare("INSERT INTO `files` (`file_url`) VALUES (:fileUrl);");
    query.bindValue(":fileUrl", fileUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query:" << query.lastError().text();
    }
}

void ChatServer::setAvatarInDatabase(const QString &avatarUrl, const int &user_id)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET avatar_url = :avatar_url WHERE id_user = :id_user;");
    query.bindValue(":id_user", user_id);
    query.bindValue(":avatar_url", avatarUrl);
    if (!query.exec()) {
        qDebug() << "Error execute sql query to setAvatarInDatabase:" << query.lastError().text();
    }
}

void ChatServer::readClient()
{
    socket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_7);

    static quint32 blockSize = 0;
    if (blockSize == 0 && socket->bytesAvailable() < sizeof(quint32)) return;
    if (blockSize == 0) in >> blockSize;
    if (socket->bytesAvailable() < blockSize) return;

    QByteArray jsonData;
    jsonData.resize(blockSize);
    in.readRawData(jsonData.data(), blockSize);

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) {
        qDebug() << "Invalid JSON-message received";
        blockSize = 0;
        return;
    }

    QJsonObject json = doc.object();
    QString flag = json["flag"].toString();

    qDebug() << "Next chech flags";
    if(flag == "login"){
        QJsonDocument sendDoc(loginProcess(json));
        sendJson(sendDoc);
    }
    else if (flag == "message") {
        QString str1 = json["str"].toString();
        QString login = json["login"].toString();
        SendToClient(doc,login);
    }
    else if(flag == "reg") {
        QJsonDocument sendDoc(regProcess(json));
        sendJson(sendDoc);
    }
    else if(flag == "logout") {
        QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
        QMultiMap<QString, QTcpSocket*>::iterator it;

        for (it = clients.begin(); it != clients.end(); ++it) {
            if (it.value() == clientSocket) {
                qDebug() << clients;

                QTcpSocket *socketValue = it.value();
                it = clients.erase(it);
                qDebug() << clients;
                break;
            }
        }
    }
    else if(flag == "search") {
        QJsonDocument sendDoc(searchProcess(json));
        sendJson(sendDoc);
    }
    else if(flag == "personal_message") personalMessageProcess(json);
    else if(flag == "updating_chats") updatingChatsProcess(json);
    else if(flag == "edit") {
        QJsonDocument sendDoc(editProfileProcess(json));
        sendJson(sendDoc);
    }
    blockSize = 0;
}

void ChatServer::SendToClient(QJsonDocument doc, const QString& senderLogin)
{
    QByteArray jsonDataOut = doc.toJson(QJsonDocument::Compact);
    data.clear();
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    for(auto it = clients.constBegin(); it != clients.constEnd(); ++it)
    {
        qDebug()<< it.key() << " << >> " << senderLogin;
        if(it.key()!= senderLogin)
        {
            qDebug() << "works if";
            it.value()->write(data);
            it.value()->flush();
        }
        else
        {
            qDebug()<< it.key();
            qDebug() << "Enter into outGoing else";
            QJsonDocument docTo = doc;
            QJsonObject jsonToOut = docTo.object();
            jsonToOut["Out"] = "out";
            QJsonDocument docToOut(jsonToOut);
            QByteArray jsonDataOutg = docToOut.toJson(QJsonDocument::Compact);

            QByteArray dataToOut;
            dataToOut.clear();
            QDataStream outg(&dataToOut,QIODevice::WriteOnly);
            outg.setVersion(QDataStream::Qt_6_7);

            outg<< quint32(jsonDataOutg.size());
            outg.writeRawData(jsonDataOutg.data(),jsonDataOutg.size());

            it.value()->write(dataToOut);
            it.value()->flush();
        }
    }
}

QString ChatServer::hashPassword(const QString &password)
{
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

bool ChatServer::checkPassword(const QString &enteredPassword, const QString &storedHash)
{
    QString hashOfEnteredPassword = hashPassword(enteredPassword);
    return hashOfEnteredPassword == storedHash;
}

void ChatServer::sendJson(const QJsonDocument &sendDoc)
{
    qDebug() << "JSON to send:" << sendDoc.toJson(QJsonDocument::Indented);
    QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    QByteArray bytes;
    bytes.clear();
    QDataStream out(&bytes,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());

    socket->write(bytes);
    socket->flush();
}

QJsonObject ChatServer::loginProcess(QJsonObject json)
{
    QString login = json["login"].toString();
    QString password = json["password"].toString();
    QJsonObject jsonLogin;
    jsonLogin["flag"] = "login";
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
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
            if (!query.exec()) {
                qDebug() << "Ошибка выполнения для получения изображения:" << query.lastError().text();
            }
            if (!query.next()) {
                qDebug() << "Нет результатов для данного запроса";
            }
            QString avatar_url = query.value(0).toString();
            int id = query.value(1).toInt();

            jsonLogin["avatar_url"] = avatar_url;
            jsonLogin["user_id"] = id;

            clients.insert(login, socket);
            qDebug() << clients;
        } else {
            jsonLogin["success"] = "poor";
        }
    }
    return jsonLogin;
}

QJsonObject ChatServer::regProcess(QJsonObject json)
{
    QString login = json["login"].toString();
    QString password = hashPassword(json["password"].toString());
    QJsonObject jsonReg;
    jsonReg["flag"] = "reg";

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", login);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
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
                qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
                jsonReg["success"] = "error";
                jsonReg["errorMes"] = "Error with insert to database";
            }
        }
    }
    return jsonReg;
}

QJsonObject ChatServer::searchProcess(QJsonObject json)
{
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
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
    }

    QJsonObject searchJson;
    searchJson["flag"] = "search";
    searchJson["results"] = jsonArray;
    return searchJson;
}

QJsonObject ChatServer::editProfileProcess(QJsonObject dataEditProfile)
{
    QString editable = dataEditProfile["editable"].toString();
    QString editInformation = dataEditProfile["editInformation"].toString();
    int user_id = dataEditProfile["user_id"].toInt();
    QSqlQuery query;
    if(editable == "Name")
    {
        query.prepare("UPDATE users SET username = :username WHERE id_user = :id_user");
        query.bindValue(":username", editInformation);
        query.bindValue(":id_user", user_id);
    }
    else if(editable == "Phone number")
    {
        query.prepare("UPDATE users SET phone_number = :phone_number WHERE id_user = :id_user");
        query.bindValue(":phone_number", editInformation);
        query.bindValue(":id_user", user_id);
    }
    else if(editable == "Username")
    {
        query.prepare("UPDATE users SET userlogin = :userlogin WHERE id_user = :id_user");
        query.bindValue(":userlogin", editInformation);
        query.bindValue(":id_user", user_id);
    }

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

void ChatServer::updatingChatsProcess(QJsonObject json)
{
    QJsonArray jsonMessageArray;
    if (json.contains("chats")) {
        QJsonArray chatHistory = json["chats"].toArray();


        for (const QJsonValue &value : chatHistory) {
            QJsonObject messageObject = value.toObject();

            int message_id = messageObject["message_id"].toInt();
            int dialog_id = messageObject["dialog_id"].toInt();


            QSqlQuery query;

            query.prepare("SELECT message_id, content, media_url, timestamp, sender_id, receiver_id "
                          "FROM messages "
                          "WHERE dialog_id = :dialog_id AND message_id > :message_id "
                          "ORDER BY timestamp");
            query.bindValue(":dialog_id", dialog_id);
            query.bindValue(":message_id", message_id);

            if (!query.exec()) {
                qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
            }
            else {
                qDebug() << "query exec true chats";
                while (query.next()) {
                    int message_id = query.value(0).toInt();
                    QString content = query.value(1).toString();
                    QString fileUrl = query.value(2).toString();
                    QString timestamp = query.value(3).toString();
                    int senderId = query.value(4).toInt();
                    int receiverId = query.value(5).toInt();
                    if(message_id + senderId + receiverId == 0){
                        continue;
                    }

                    QString senderLogin;
                    QString receiverLogin;
                    QString sender_avatar_url;
                    QString receiver_avatar_url;

                    QSqlQuery senderQuery;
                    senderQuery.prepare("SELECT userlogin, avatar_url FROM users WHERE id_user = :sender_id");
                    senderQuery.bindValue(":sender_id", senderId);
                    if (senderQuery.exec() && senderQuery.next()) {
                        senderLogin = senderQuery.value(0).toString();
                        sender_avatar_url = senderQuery.value(1).toString();
                    }
                    QSqlQuery receiverQuery;
                    receiverQuery.prepare("SELECT userlogin, avatar_url FROM users WHERE id_user = :receiver_id");
                    receiverQuery.bindValue(":receiver_id", receiverId);
                    if (receiverQuery.exec() && receiverQuery.next()) {
                        receiverLogin = receiverQuery.value(0).toString();
                        receiver_avatar_url = receiverQuery.value(1).toString();
                    }

                    QJsonObject messageObject;
                    messageObject["FullDate"] = timestamp;
                    messageObject["receiver_login"] = receiverLogin;
                    messageObject["receiver_avatar_url"] = receiver_avatar_url;
                    messageObject["sender_login"] = senderLogin;
                    messageObject["sender_avatar_url"] = sender_avatar_url;
                    messageObject["receiver_id"] = receiverId;
                    messageObject["sender_id"] = senderId;
                    messageObject["message_id"] = message_id;
                    messageObject["dialog_id"] = dialog_id;
                    messageObject["str"] = content;
                    messageObject["fileUrl"] = fileUrl;
                    if(dialog_id == 0) {continue;}

                    QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
                    messageObject["time"] = dateTime.toString("hh:mm");

                    jsonMessageArray.append(messageObject);
                }
            }
        }
        QJsonObject updatingJson;
        updatingJson["flag"] = "updating_chats";
        updatingJson["messages"] = jsonMessageArray;
        if(!jsonMessageArray.isEmpty()){
            QJsonDocument sendDoc(updatingJson);
            sendJson(sendDoc);
        }
    }
    if (json.contains("userlogin")){
        QString userlogin = json["userlogin"].toString();
        QJsonArray dialogIdsArray= json["dialogIds"].toArray();
        int user_id = -1;

        QSqlQuery query;

        query.prepare("SELECT id_user FROM users WHERE userlogin = :userlogin");
        query.bindValue(":userlogin", userlogin);
        if (query.exec() && query.next()) {
            user_id = query.value(0).toInt();
        } else {
            qDebug() << "Не удалось найти user_id для userlogin " << userlogin << query.lastError().text();
            return;
        }

        QSqlQuery dialogQuery;
        dialogQuery.prepare("SELECT dialog_id FROM messages WHERE sender_id = :user_id OR receiver_id = :user_id");
        dialogQuery.bindValue(":user_id", user_id);

        QList<int> dialogIds;
        if (dialogQuery.exec()) {
            while (dialogQuery.next()) {
                int dialogId = dialogQuery.value(0).toInt();
                if(!dialogIds.contains(dialogId)){
                    dialogIds.append(dialogId);
                }
            }
        } else {
            qDebug() << "Ошибка получения диалогов для user_id" << user_id << dialogQuery.lastError().text();
        }

        QSet<int> idsToDelete;
        for (const QJsonValue &value : dialogIdsArray) {
            idsToDelete.insert(value.toInt());
        }
        dialogIds.erase(std::remove_if(dialogIds.begin(), dialogIds.end(), [&idsToDelete](int id) {
                            return idsToDelete.contains(id);
                        }), dialogIds.end());
        qDebug() << dialogIds;

        for (int dialog_id : dialogIds) {
            QSqlQuery query;
            query.prepare("SELECT message_id, content, media_url timestamp, sender_id, receiver_id "
                          "FROM messages "
                          "WHERE dialog_id = :dialog_id "
                          "ORDER BY timestamp");
            query.bindValue(":dialog_id", dialog_id);

            if (!query.exec()) {
                qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
            } else {
                qDebug() << "query exec true userlogin";

                while (query.next()) {
                    int message_id = query.value(0).toInt();
                    QString content = query.value(1).toString();
                    QString fileUrl = query.value(2).toString();
                    QString timestamp = query.value(3).toString();
                    int senderId = query.value(4).toInt();
                    int receiverId = query.value(5).toInt();

                    QString senderLogin;
                    QString receiverLogin;
                    QString sender_avatar_url;
                    QString receiver_avatar_url;

                    QSqlQuery senderQuery;
                    senderQuery.prepare("SELECT userlogin, avatar_url FROM users WHERE id_user = :sender_id");
                    senderQuery.bindValue(":sender_id", senderId);
                    if (senderQuery.exec() && senderQuery.next()) {
                        senderLogin = senderQuery.value(0).toString();
                        sender_avatar_url = senderQuery.value(1).toString();
                    }
                    QSqlQuery receiverQuery;
                    receiverQuery.prepare("SELECT userlogin, avatar_url FROM users WHERE id_user = :receiver_id");
                    receiverQuery.bindValue(":receiver_id", receiverId);
                    if (receiverQuery.exec() && receiverQuery.next()) {
                        receiverLogin = receiverQuery.value(0).toString();
                        receiver_avatar_url = receiverQuery.value(1).toString();
                    }

                    QJsonObject messageObject;
                    messageObject["FullDate"] = timestamp;
                    messageObject["receiver_login"] = receiverLogin;
                    messageObject["receiver_avatar_url"] = receiver_avatar_url;
                    messageObject["sender_login"] = senderLogin;
                    messageObject["sender_avatar_url"] = sender_avatar_url;
                    messageObject["receiver_id"] = receiverId;
                    messageObject["sender_id"] = senderId;
                    messageObject["message_id"] = message_id;
                    messageObject["dialog_id"] = dialog_id;
                    messageObject["str"] = content;
                    messageObject["fileUrl"] = fileUrl;

                    QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
                    messageObject["time"] = dateTime.toString("hh:mm");
                    if(dialog_id == 0) {continue;}

                    jsonMessageArray.append(messageObject);
                }
            }
        }
        QJsonObject updatingJson;
        updatingJson["flag"] = "updating_chats";
        updatingJson["messages"] = jsonMessageArray;
        if(!jsonMessageArray.isEmpty()){
            QJsonDocument sendDoc(updatingJson);
            sendJson(sendDoc);
        }
    }

}

void ChatServer::personalMessageProcess(QJsonObject json)
{
    int receiver_id = json["receiver_id"].toInt();
    QString receiver_login = json["receiver_login"].toString();
    int sender_id =  json["sender_id"].toInt();
    QString sender_login =  json["sender_login"].toString();
    QString message =  json["message"].toString();


    QString receiver_avatar_url = getAvatarUrl(receiver_login);
    QString sender_avatar_url = getAvatarUrl(sender_login);
    json["receiver_avatar_url"] = receiver_avatar_url;
    json["sender_avatar_url"] = sender_avatar_url;

    int dialog_id = getOrCreateDialog(sender_id, receiver_id);
    int message_id;

    if(json.contains("fileUrl")) {
        QString fileUrl =  json["fileUrl"].toString();
        message_id = saveMessageToDatabase(dialog_id, sender_id,receiver_id, message, fileUrl);
    } else {
        message_id = saveMessageToDatabase(dialog_id, sender_id,receiver_id, message);
    }
    sendMessageToActiveSockets(json,message_id, dialog_id);
}

QString ChatServer::getAvatarUrl(const QString &userlogin)
{
    qDebug() << "getAvatarUrl starts";
    QSqlQuery query;
    query.prepare("SELECT avatar_url FROM users WHERE userlogin = :userlogin");
    query.bindValue(":userlogin", userlogin);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    } else {
        qDebug() << "query getAvatarUrl error: " << query.lastError().text();
    }
    return "";
}

int ChatServer::getOrCreateDialog(int sender_id, int receiver_id)
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

int ChatServer::saveMessageToDatabase(int dialogId, int senderId,int receiverId, const QString &message,const QString& fileUrl)
{
    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender_id, receiver_id, dialog_id, content, media_url) VALUES (:sender_id, :receiver_id, :dialog_id, :content, :fileUrl)");
    query.bindValue(":sender_id", senderId);
    query.bindValue(":receiver_id", receiverId);
    query.bindValue(":dialog_id", dialogId);
    query.bindValue(":content", message);
    query.bindValue(":fileUrl", fileUrl);
    if (query.exec()) {

    } else {
        qDebug() << "Error exec query:" << query.lastError().text();
    }
    return query.lastInsertId().toInt();
}

void ChatServer::sendMessageToActiveSockets(QJsonObject json, int message_id, int dialog_id)
{
    QString sender_login =  json["sender_login"].toString();
    QString receiver_login = json["receiver_login"].toString();

    QJsonObject messageJson;
    messageJson["flag"] = "personal_message";
    messageJson["message_id"] = message_id;
    messageJson["message"] = json["message"];
    messageJson["dialog_id"] = dialog_id;
    messageJson["sender_login"] = json["sender_login"];
    messageJson["sender_avatar_url"] = json["sender_avatar_url"];
    messageJson["sender_id"] = json["sender_id"];
    messageJson["fileUrl"] = json["fileUrl"];
    messageJson["time"] = QDateTime::currentDateTime().toString("HH:mm");

    QJsonDocument doc(messageJson);
    QByteArray jsonDataOut = doc.toJson(QJsonDocument::Compact);
    data.clear();
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    if (clients.contains(sender_login)) {
        qDebug() << "sender online";

        QJsonDocument docTo = doc;
        QJsonObject jsonToOut = docTo.object();
        jsonToOut.remove("sender_login");

        int sender_id = jsonToOut["sender_id"].toInt();

        jsonToOut.remove("sender_avatar_url");
        jsonToOut.remove("sender_id");
        jsonToOut["receiver_login"] = json["receiver_login"];
        jsonToOut["receiver_id"] = json["receiver_id"];
        jsonToOut["receiver_avatar_url"] = json["receiver_avatar_url"];

        int receiver_id = jsonToOut["receiver_id"].toInt();

        qDebug() <<  receiver_id;
        QJsonDocument docToOut(jsonToOut);
        QByteArray jsonDataOutg = docToOut.toJson(QJsonDocument::Compact);

        QByteArray dataToOut;
        dataToOut.clear();
        QDataStream outg(&dataToOut,QIODevice::WriteOnly);
        outg.setVersion(QDataStream::Qt_6_7);

        outg<< quint32(jsonDataOutg.size());
        outg.writeRawData(jsonDataOutg.data(),jsonDataOutg.size());

        QList<QTcpSocket*> sender_socketList = clients.values(sender_login);
        for (QTcpSocket* socket : sender_socketList) {
            socket->write(dataToOut);
            socket->flush();
        }

        if(sender_login == receiver_login and sender_id == receiver_id)
        {
            return;
        }
    } else {
        qDebug() << "sender offline";
    }
    if (clients.contains(receiver_login)) {

        qDebug() << "receiver online";

        qDebug() <<  messageJson["sender_id"].toInt();
        QList<QTcpSocket*> receiver_socketList = clients.values(receiver_login);
        for (QTcpSocket* socket : receiver_socketList) {
            socket->write(data);
            socket->flush();
        }

    } else {
        qDebug() << "receiver offline";
    }
}

void ChatServer::connectToDB()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMARIADB");
    db.setHostName("localhost");
    db.setPort(3306);
    db.setDatabaseName("test_db");
    db.setUserName("admin");
    db.setPassword("admin-password");
    if(db.open())
    {
        qDebug() << "Database is connected";
    }
    else
    {
        qDebug() << "Available drivers:" << QSqlDatabase::drivers();
        QSqlError error = db.lastError();
        qDebug() << "Error connecting to database:" << error.text();
    }
}

void ChatServer::disconnectClient()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    qDebug()<< "Client disconnect";
    if(clientSocket)
    {
        QString loginToRemove;
        for (auto it = clients.begin(); it!= clients.end();it++)
        {
            if(it.value() == clientSocket)
            {
                loginToRemove =it.key();
                break;
            }
        }
        clients.remove(loginToRemove);
        clientSocket->deleteLater();
    }
}


