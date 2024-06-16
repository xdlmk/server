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
}

void ChatServer::incomingConnection(qintptr handle)
{
    socket = new QTcpSocket(this);
    if(socket->setSocketDescriptor(handle))
    {
        clients << socket;
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


        if(flag == "login")
        {
            QString login = json["login"].toString();
            QString password = json["password"].toString();
            QJsonObject jsonLogin;
            jsonLogin["flag"] = "login";
            QSqlQuery query;
            query.prepare("SELECT COUNT(*) FROM users WHERE userLogin = :userLogin AND userPassword = :userPassword");
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
                } else {
                    jsonLogin["success"] = "poor";
                }
            }
                QJsonDocument sendDoc(jsonLogin);
                qDebug() << "JSON to send:" << sendDoc.toJson(QJsonDocument::Indented); // Вывод JSON с отступами для читаемости
                QByteArray bytes;
                QDataStream out(&bytes,QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_7);
                out << sendDoc.toJson();
                socket->write(bytes);
        }
        else if (flag == "message")
        {
            QString str1 = json["str"].toString();
            qDebug() << "Message:" << str1;
            SendToClient(doc,socket);
        }
    }
    else qDebug() << "DataStream error: 37 chatserver.cpp";

}

void ChatServer::SendToClient(QJsonDocument doc, QTcpSocket* socket)
{
    data.clear();
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out << doc.toJson();
    for( QTcpSocket* client : clients)
    {
        if(client != socket)
        {
        client->write(data);
        }
    }
}

void ChatServer::disconnectClient()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if(clientSocket)
    {
        clients.removeAll(clientSocket);
        clientSocket->deleteLater();
    }
}


