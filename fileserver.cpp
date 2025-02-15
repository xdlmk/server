#include "fileserver.h"
#include "qjsonobject.h"

FileServer::FileServer(QObject *parent)
    : QTcpServer{parent}
{
    if(!this->listen(QHostAddress::Any,2021))
    {
        qInfo() <<"Unable to start the FileServer: " << this->errorString();
    }
    else
    {
        qInfo() << "FileServer started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
    }
}

void FileServer::incomingConnection(qintptr handle)
{
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(handle);

    connect(socket, &QTcpSocket::readyRead, this,&FileServer::readClient);

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void FileServer::sendData(QJsonDocument &sendDoc)
{
    QJsonObject readJson = sendDoc.object();
    qDebug() << "Flag FileJson to send:" << readJson["flag"].toString();
    qDebug() << "Flag user_id to send:" << readJson["user_id"].toInt();
    qDebug() << "Flag avatarUrl to send:" << readJson["avatar_url"].toString();
    QByteArray jsonDataOut = sendDoc.toJson(QJsonDocument::Compact);
    QByteArray data;
    data.clear();
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    socket->write(data);
    qInfo() << "File send";
    socket->flush();
}

void FileServer::readClient()
{
    qInfo() << "Get message fileServer!";
    socket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_7);

    static quint32 blockSize = 0;

    if(in.status()== QDataStream::Ok)
    {
        if (blockSize == 0) {
            if (socket->bytesAvailable() < sizeof(quint32))
            {
                return;
            }
            in >> blockSize;
            qDebug() << "Block size:" << blockSize;
        }

        if (socket->bytesAvailable() < blockSize)
        {
            return;
        }

        QByteArray data;
        data.resize(blockSize);
        in.readRawData(data.data(), blockSize);

        QJsonDocument doc = QJsonDocument::fromJson(data);

        if (doc.isNull()) {
            qDebug() << "Error with JSON doc check, doc is null";
            blockSize = 0;
            return;
        }

        QJsonObject json = doc.object();
        if (json["flag"].toString() == "avatar") {
            getAvatarFromServer(json);
        } else if (json.contains("fileName")) {
            makeUrlProcessing(json);
        } else if (json.contains("fileUrl")){
            getFileFromUrlProcessing(json["fileUrl"].toString());
        }

    }
    blockSize = 0;
}

void FileServer::makeUrlProcessing(const QJsonObject &json)
{
    qDebug() << "makeUrlProcessing starts";
    QString fileName = json["fileName"].toString();
    QString fileExtension = json["fileExtension"].toString();
    QString fileDataBase64 = json["fileData"].toString();
    QByteArray fileData = QByteArray::fromBase64(fileDataBase64.toUtf8());

    QString uniqueFileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + fileName + "." + fileExtension;

    QDir dir("uploads");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file("uploads/" + uniqueFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save file:" << uniqueFileName;
        return;
    }

    file.write(fileData);
    file.close();

    qDebug() << "File saved as:" << uniqueFileName;
    emit saveFileToDatabase(uniqueFileName);

    QJsonObject fileUrl;
    fileUrl["flag"] = "fileUrl";
    fileUrl["fileUrl"] = uniqueFileName;

    QJsonDocument sendDoc(fileUrl);
    sendData(sendDoc);
}

void FileServer::getFileFromUrlProcessing(const QString &fileUrl)
{
    qDebug() << "getFileFromUrlProcessing starts";
    QFile file("uploads/" + fileUrl);
    QByteArray fileData;
    if(file.open(QIODevice::ReadOnly)) {
        fileData = file.readAll();
        file.close();
    }
    QJsonObject fileDataJson;
    fileDataJson["flag"] = "fileData";
    fileDataJson["fileName"] = fileUrl;
    fileDataJson["fileData"] = QString(fileData.toBase64());
    QJsonDocument doc(fileDataJson);

    sendData(doc);
}

void FileServer::getAvatarFromServer(const QJsonObject &json)
{
    qDebug() << "getAvatarFromServer starts";
    QDir dir("avatars");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file("avatars/" + json["avatar_url"].toString());

    QByteArray fileData;
    if(file.open(QIODevice::ReadOnly)) {
        fileData = file.readAll();
        file.close();
    }
    QJsonObject fileDataJson;
    fileDataJson["flag"] = "avatarData";
    fileDataJson["user_id"] = json["user_id"].toInt();
    fileDataJson["avatar_url"] = json["avatar_url"].toString();
    fileDataJson["avatarData"] = QString(fileData.toBase64());
    QJsonDocument doc(fileDataJson);

    sendData(doc);
}
