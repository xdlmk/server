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

void ChatServer::sendJson(const QJsonDocument &sendDoc)
{
    qDebug() << "JSON to send:" << sendDoc.toJson(QJsonDocument::Indented);
    QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    qDebug() << "Отправляемые данные в байтах:" << jsonData;

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
    query.prepare("SELECT COUNT(*) FROM users2 WHERE userLogin = :userLogin AND userPassword = :userPassword");
    query.bindValue(":userLogin", login);
    query.bindValue(":userPassword", password);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
        jsonLogin["success"] = "error";
    }
    else {
        query.next();
        int count = query.value(0).toInt();
        if (count > 0) {
            jsonLogin["success"] = "ok";
            jsonLogin["name"] = json["login"];
            jsonLogin["password"] = json["password"];

            query.prepare("SELECT userProfileImage FROM users2 WHERE userLogin = :userLogin AND userPassword = :userPassword");
            query.bindValue(":userLogin", login);
            query.bindValue(":userPassword", password);
            if (!query.exec()) {
                qDebug() << "Ошибка выполнения для получения изображения:" << query.lastError().text();
            }
            if (!query.next()) {
                qDebug() << "Нет результатов для данного запроса";
            }
            QByteArray imageData = query.value(0).toByteArray();

            QString base64ImageData = QString::fromLatin1(imageData.toBase64());

            jsonLogin["profileImage"] = base64ImageData;
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
    QString password = json["password"].toString();
    QJsonObject jsonReg;
    jsonReg["flag"] = "reg";

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users2 WHERE userLogin = :userLogin");
    query.bindValue(":userLogin", login);

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

            QFile file("resources/images/defaultAvatar.png");
            if (!file.open(QIODevice::ReadOnly)) {
                qDebug() << "Error: could not open file for reading";
            }
            else {
                QByteArray imageData = file.readAll();
                file.close();

                query.prepare("INSERT INTO `blockgram`.`users2` (`userLogin`, `userPassword`, `userName`, `userProfileImage`) VALUES (:userLogin, :userPassword, :userLogin, :image);");
                query.bindValue(":userLogin", login);
                query.bindValue(":userPassword",password);
                query.bindValue(":image",imageData);
                jsonReg["name"] = json["login"];
                jsonReg["password"] = json["password"];
                if (!query.exec()) {
                    qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
                    jsonReg["success"] = "error";
                    jsonReg["errorMes"] = "Error with insert to database";
                }
            }
        }
    }
    return jsonReg;
}

void ChatServer::connectToDB()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setDatabaseName("blockgram");
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


