#include "chatserver.h"

ChatServer::ChatServer(QObject *parent)
    : QTcpServer{parent}
{
    if(!this->listen(QHostAddress::Any,2020))
    {
        qInfo() <<"Unable to start the server: " << this->errorString();
    }
    else
    {
        qInfo() << "Server started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
        connectToDB();
    }
}

void ChatServer::incomingConnection(qintptr handle)
{
    socket = new QTcpSocket(this);
    if(socket->setSocketDescriptor(handle))
    {
        connect(socket,&QTcpSocket::readyRead,this,&ChatServer::readClient);
        connect(socket,&QTcpSocket::disconnected,this,&ChatServer::disconnectClient);
        qDebug() << "Client connect: " << handle;
    }
    else
    {
        delete socket;
    }
}

void ChatServer::readClient()
{
    qInfo() << "Get message!";
    socket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_7);

    if(in.status()== QDataStream::Ok)
    {
        qDebug() << "read..";
        QByteArray str;
        in>>str;
        QJsonDocument doc = QJsonDocument::fromJson(str);
        QJsonObject json = doc.object();
        QString flag = json["flag"].toString();

        qDebug() << "Next chech flags";
        if(flag == "login")
        {
            qDebug() << "Flag == login";

            QJsonDocument sendDoc(loginProcess(json));
            sendJson(sendDoc);
        }
        else if (flag == "message")
        {
            qDebug() << "Flag == message ";

            QString str1 = json["str"].toString();
            QString login = json["login"].toString();
            qDebug() << "Message:" << str1 << " from " << login;
            SendToClient(doc,login);
        }
        else if(flag == "reg")
        {
            qDebug() << "Flag == reg";

            QJsonDocument sendDoc(regProcess(json));
            sendJson(sendDoc);
        }
        else if(flag == "logout")
        {
            QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
            qDebug()<< "Client logout";
            qDebug() << clients;

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
    }
    else qDebug() << "DataStream error: 37 chatserver.cpp";

}

void ChatServer::SendToClient(QJsonDocument doc, const QString& senderLogin)
{
    QByteArray jsonDataOut = doc.toJson(QJsonDocument::Compact);
    data.clear();
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    //out << doc.toJson();
    for(auto it = clients.constBegin(); it != clients.constEnd(); ++it)
    {
        qDebug()<< it.key() << "   5" << senderLogin;
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

            //outg << docToOut.toJson();

            it.value()->write(dataToOut);
            it.value()->flush();
        }
    }
}

QString ChatServer::convertImageToBase64(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "File not open:" << file.errorString();
        return QString();
    }
    QByteArray imageData = file.readAll();
    QString base64ImageData = QString::fromLatin1(imageData.toBase64());

    file.close();
    return base64ImageData;
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
    QJsonObject jsonToSend = sendDoc.object();
    jsonToSend.remove("profileImage");
    QJsonDocument toSendDoc(jsonToSend);
    qDebug() << "(without profileImage)JSON to send:" << toSendDoc.toJson(QJsonDocument::Indented);


    QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    //qDebug() << "Отправляемые данные в байтах:" << jsonData;

    QByteArray bytes;
    bytes.clear();
    QDataStream out(&bytes,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    ////////////////////
    //QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());
    ///////////////
    //out << sendDoc.toJson();


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

            query.prepare("SELECT avatar_url FROM users WHERE userlogin = :userlogin");
            query.bindValue(":userlogin", login);
            if (!query.exec()) {
                qDebug() << "Ошибка выполнения для получения изображения:" << query.lastError().text();
            }
            if (!query.next()) {
                qDebug() << "Нет результатов для данного запроса";
            }
            QString avatar_url = query.value(0).toString();

            jsonLogin["profileImage"] = convertImageToBase64(avatar_url);
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

            QString default_avatar_url = "images/defaultAvatar.png";

                query.prepare("INSERT INTO `users` (`username`, `password_hash`, `userlogin`, `avatar_url`) VALUES (:username, :password_hash, :username, :avatar_url);");
                query.bindValue(":username", login);
                query.bindValue(":password_hash",password);
                query.bindValue(":avatar_url",default_avatar_url);
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

void ChatServer::connectToDB()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
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


