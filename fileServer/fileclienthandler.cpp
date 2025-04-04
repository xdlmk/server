#include "fileclienthandler.h"

FileClientHandler::FileClientHandler(quintptr handle, QObject *parent)
    : QObject{parent}, fileSocket(new QTcpSocket(this)), logger(Logger::instance()) {
    if(fileSocket->setSocketDescriptor(handle)) {
        connect(fileSocket, &QTcpSocket::readyRead, this, &FileClientHandler::readClient);
        connect(fileSocket, &QTcpSocket::disconnected, this, &FileClientHandler::disconnectClient);
        connect(fileSocket, &QTcpSocket::bytesWritten, this, &FileClientHandler::handleBytesWritten);
    }
}

void FileClientHandler::readClient()
{
    fileSocket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(fileSocket);
    in.setVersion(QDataStream::Qt_6_7);

    static quint32 blockSize = 0;
    if (blockSize == 0 && fileSocket->bytesAvailable() < sizeof(quint32)) return;
    if (blockSize == 0) in >> blockSize;
    if (fileSocket->bytesAvailable() < blockSize) return;

    QByteArray data;
    data.resize(blockSize);
    in.readRawData(data.data(), blockSize);

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        logger.log(Logger::DEBUG,"fileclienthandler.cpp::readClient", "Error with JSON doc check, doc is null");
        blockSize = 0;
        return;
    }
    QJsonObject json = doc.object();

    processClientRequest(json);

    blockSize = 0;
}

void FileClientHandler::processClientRequest(const QJsonObject &json)
{
    auto it = flagMap.find(json["flag"].toString().toStdString());
    uint flagId = (it != flagMap.end()) ? it->second : 0;
    switch (flagId) {
    case 1:
        //sendData(fileHandler.getAvatarFromServer(json));
        break;
    case 2:
        if (json["type"].toString() == "personal") {
            //sendData(fileHandler.makeAvatarUrlProcessing(json));
        } else if (json["type"].toString() == "group") {
            //emit sendNewGroupAvatarUrlToActiveSockets(fileHandler.makeAvatarUrlProcessing(json));
        }
        break;
    case 3:
    case 4: {
        QJsonObject fileUrlJson;
        fileUrlJson["flag"] = json["flag"].toString() + "_url";
        //fileUrlJson["fileUrl"] = fileHandler.makeUrlProcessing(json);
        //sendData(fileUrlJson);
        break;
    }
    case 5:
        //sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(), "fileData"));
        break;
    case 6:
        //sendData(fileHandler.getFileFromUrlProcessing(json["fileUrl"].toString(), "voiceFileData"));
        break;
    case 7:
    case 8: {
        QJsonObject voiceMessage = json;
        //fileHandler.voiceMessageProcessing(voiceMessage);
        //emit sendVoiceMessage(voiceMessage);
        break;
    }
    case 9: {
        QJsonObject createGroupJson = json;
        //fileHandler.createGroupWithAvatarProcessing(createGroupJson);
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
