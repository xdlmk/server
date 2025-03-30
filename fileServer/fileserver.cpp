#include "fileserver.h"
#include "qjsonobject.h"

FileServer::FileServer(QObject *parent) : QTcpServer{parent}, logger(Logger::instance()) {
    if(!this->listen(QHostAddress::Any,2021)) {
        logger.log(Logger::FATAL,"fileserver.cpp::constructor", "Unable to start the FileServer: " + this->errorString());
    } else {
        logger.log(Logger::INFO,"fileserver.cpp::constructor", "FileServer started and listening adresses: " + this->serverAddress().toString() + " on port: " + QString::number(this->serverPort()));
    }
    connect(&fileHandler,&FileHandler::saveFileToDatabase,this,&FileServer::saveFileToDatabase);
    connect(&fileHandler,&FileHandler::setAvatarInDatabase,this,&FileServer::setAvatarInDatabase);
    connect(&fileHandler,&FileHandler::setGroupAvatarInDatabase,this,&FileServer::setGroupAvatarInDatabase);
    connect(&fileHandler,&FileHandler::createGroup,this,&FileServer::createGroup);
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
        logger.log(Logger::DEBUG,"fileserver.cpp::readClient", "Error with JSON doc check, doc is null");
        blockSize = 0;
        return;
    }
    QJsonObject json = doc.object();

    processClientRequest(json);

    blockSize = 0;
}

void FileServer::sendData(const QJsonObject &sendJson)
{
    logger.log(Logger::DEBUG,"fileserver.cpp::sendData", "Flag FileJson to send: " + sendJson["flag"].toString());
    QJsonDocument sendDoc(sendJson);
    QByteArray jsonDataOut = sendDoc.toJson(QJsonDocument::Compact);
    QByteArray data;
    QDataStream out(&data,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);
    out<<quint32(jsonDataOut.size());
    out.writeRawData(jsonDataOut.data(),jsonDataOut.size());

    socket->write(data);
    socket->flush();
}

void FileServer::processClientRequest(const QJsonObject &json)
{
    auto it = flagMap.find(json["flag"].toString().toStdString());
    uint flagId = (it != flagMap.end()) ? it->second : 0;
    switch (flagId) {
    case 1:
        sendData(fileHandler.getAvatarFromServer(json));
        break;
    case 2:
        if (json["type"].toString() == "personal") {
            sendData(fileHandler.makeAvatarUrlProcessing(json));
        } else if (json["type"].toString() == "group") {
            emit sendNewGroupAvatarUrlToActiveSockets(fileHandler.makeAvatarUrlProcessing(json));
        }
        break;
    case 3:
    case 4: {
        QJsonObject fileUrlJson;
        fileUrlJson["flag"] = json["flag"].toString() + "_url";
        fileUrlJson["fileUrl"] = fileHandler.makeUrlProcessing(json);
        sendData(fileUrlJson);
        break;
    }
    case 5:
        sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(), "fileData"));
        break;
    case 6:
        sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(), "voiceFileData"));
        break;
    case 7:
    case 8: {
        QJsonObject voiceMessage = json;
        fileHandler.voiceMessageProcessing(voiceMessage);
        emit sendVoiceMessage(voiceMessage);
        break;
    }
    case 9: {
        QJsonObject createGroupJson = json;
        fileHandler.createGroupWithAvatarProcessing(createGroupJson);
        break;
    }
    default:
        logger.log(Logger::WARN,"fileserver.cpp::processClientRequest", "Unknown flag: " + json["flag"].toString());
        break;
    }
}

const std::unordered_map<std::string_view, uint> FileServer::flagMap = {
    {"avatarUrl", 1}, {"newAvatarData", 2},
    {"personal_file", 3}, {"group_file", 4},
    {"fileUrl", 5}, {"voiceFileUrl", 6},
    {"personal_voice_message", 7}, {"group_voice_message", 8},
    {"create_group", 9}
};
