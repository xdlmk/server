#include "fileserver.h"
#include "qjsonobject.h"

FileServer::FileServer(QObject *parent) : QTcpServer{parent} {
    if(!this->listen(QHostAddress::Any,2021)) {
        qInfo() <<"Unable to start the FileServer: " << this->errorString();
    } else {
        qInfo() << "FileServer started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
    }
    connect(&fileHandler,&FileHandler::saveFileToDatabase,this,&FileServer::saveFileToDatabase);
    connect(&fileHandler,&FileHandler::setAvatarInDatabase,this,&FileServer::setAvatarInDatabase);
}

void FileServer::incomingConnection(qintptr handle) {
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(handle);
    connect(socket, &QTcpSocket::readyRead, this,&FileServer::readClient);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void FileServer::readClient()
{
    qInfo() << "Get message fileServer!";
    socket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_7);

    static quint32 blockSize = 0;
    if (blockSize == 0 && socket->bytesAvailable() < sizeof(quint32)) return;
    if (blockSize == 0) in >> blockSize;
    if (socket->bytesAvailable() < blockSize) return;

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

    processClientRequest(json);

    blockSize = 0;
}

void FileServer::sendData(const QJsonObject &sendJson)
{
    qDebug() << "Flag FileJson to send:" << sendJson["flag"].toString();
    QJsonDocument sendDoc(sendJson);
    QByteArray jsonDataOut = sendDoc.toJson(QJsonDocument::Compact);
    QByteArray data;
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    if(socket->write(data)) qInfo() << "File sent successfully";
    socket->flush();
}

void FileServer::processClientRequest(const QJsonObject &json)
{
    if (json["flag"].toString() == "avatarUrl") sendData(fileHandler.getAvatarFromServer(json));
    else if (json["flag"].toString() == "newAvatarData") sendData(fileHandler.makeAvatarUrlProcessing(json));
    else if (json["flag"].toString() == "personal_file" || json["flag"].toString() == "group_file") {
        QString fileUrl = fileHandler.makeUrlProcessing(json);
        QJsonObject fileUrlJson;
        fileUrlJson["flag"] = json["flag"].toString()+"_url";
        fileUrlJson["fileUrl"] = fileUrl;
        sendData(fileUrlJson);
    }
    else if (json["flag"].toString() == "fileUrl") sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(),"fileData"));
    else if (json["flag"].toString() == "voiceFileUrl") sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(),"voiceFileData"));
    else if(json["flag"].toString() == "voice_message") {
        QJsonObject voiceMessage = json;
        fileHandler.voiceMessageProcessing(voiceMessage);
        emit sendVoiceMessage(voiceMessage);
    }
}
