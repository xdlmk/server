#include "fileclienthandler.h"
#include "filehandler.h"

FileClientHandler::FileClientHandler(quintptr handle, FileServer *server, QObject *parent)
    : QObject{parent}, fileSocket(new QTcpSocket(this)), logger(Logger::instance()) {
    if(fileSocket->setSocketDescriptor(handle)) {
        connect(fileSocket, &QTcpSocket::readyRead, this, &FileClientHandler::readClient);
        connect(fileSocket, &QTcpSocket::disconnected, this, &FileClientHandler::disconnectClient);
        connect(fileSocket, &QTcpSocket::bytesWritten, this, &FileClientHandler::handleBytesWritten);

        logger.log(Logger::INFO,"fileclienthandler.cpp::constructor", QString("Client connect: ") + QString::number(handle));
        this->server = server;
    } else {
        delete fileSocket;
        emit clientDisconnected(this);
    }
}

bool FileClientHandler::setIdentifiers(const QString &login, const int &id)
{
    logger.log(Logger::INFO,"fileclientHandler.cpp::setIdentifiers", "Set login: "  + login + ", set id: " + QString::number(id));
    this->login = login;
    this->id = id;
    return !this->login.isEmpty() && this->id > 0;
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

void FileClientHandler::handleBytesWritten(qint64 bytes)
{
    QMutexLocker lock(&mutex);

    logger.log(Logger::INFO, "fileclienthandler.cpp::handleBytesWritten", "Bytes written: " + QString::number(bytes));

    if (!sendQueue.isEmpty()) {
        sendQueue.dequeue();
        logger.log(Logger::INFO, "fileclienthandler.cpp::handleBytesWritten", "Message dequeued. Queue size: " + QString::number(sendQueue.size()));
        if (!sendQueue.isEmpty()) {
            logger.log(Logger::INFO, "fileclienthandler.cpp::handleBytesWritten", "More messages in queue. Scheduling next processing.");
            QTimer::singleShot(10, this, &FileClientHandler::processSendQueue);
        }
    }
}

void FileClientHandler::disconnectClient()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if(clientSocket) {
        emit clientDisconnected(this);
        clientSocket->deleteLater();
    }
}

void FileClientHandler::sendData(const QJsonObject &jsonToSend)
{
    QJsonDocument sendDoc(jsonToSend);
    if(jsonToSend.contains("avatarData")){
        QJsonObject sendJson = jsonToSend;
        sendJson["avatarData"] = "here avatar data";
        QJsonDocument sendDoc2(sendJson);
        logger.log(Logger::INFO,"fileclienthandler.cpp::sendData", "JSON to send (compact):" + sendDoc2.toJson(QJsonDocument::Compact));
    } else if(jsonToSend.contains("fileData")) {
        QJsonObject sendJson = jsonToSend;
        sendJson["fileData"] = "here file data";
        QJsonDocument sendDoc3(sendJson);
        logger.log(Logger::INFO,"fileclienthandler.cpp::sendData", "JSON to send (compact):" + sendDoc3.toJson(QJsonDocument::Compact));
    } else {
        logger.log(Logger::INFO,"fileclienthandler.cpp::sendData", "JSON to send (compact):" + sendDoc.toJson(QJsonDocument::Compact));
    }
    QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    bool shouldStartProcessing = false;
    {
        QMutexLocker lock(&mutex);
        if (sendQueue.size() >= MAX_QUEUE_SIZE) {
            logger.log(Logger::DEBUG, "fileclienthandler.cpp::sendJson", "Send queue overflow! Dropping message.");
            return;
        }
        sendQueue.enqueue(jsonData);
        logger.log(Logger::INFO, "fileclienthandler.cpp::sendJson", "Message added to queue. Queue size: " + QString::number(sendQueue.size()));
        shouldStartProcessing = sendQueue.size() == 1;
    }

    if (shouldStartProcessing) {
        logger.log(Logger::INFO, "fileclienthandler.cpp::sendJson", "Starting to process the send queue.");
        processSendQueue();
    }
}

void FileClientHandler::processClientRequest(const QJsonObject &json)
{
    auto it = flagMap.find(json["flag"].toString().toStdString());
    uint flagId = (it != flagMap.end()) ? it->second : 0;
    switch (flagId) {
    case 1:
        sendData(server->getFileHandler()->getAvatarFromServer(json));
        break;
    case 2:
        if (json["type"].toString() == "personal") {
            sendData(server->getFileHandler()->makeAvatarUrlProcessing(json));
        } else if (json["type"].toString() == "group") {
            server->sendNewGroupAvatarUrlToActiveSockets(server->getFileHandler()->makeAvatarUrlProcessing(json));
        }
        break;
    case 3:
    case 4: {
        QJsonObject fileUrlJson;
        fileUrlJson["flag"] = json["flag"].toString() + "_url";
        fileUrlJson["fileUrl"] = server->getFileHandler()->makeUrlProcessing(json);
        sendData(fileUrlJson);
        break;
    }
    case 5:
        sendData(server->getFileHandler()->getFileFromUrlProcessing(json["fileUrl"].toString(), "fileData"));
        break;
    case 6:
        sendData(server->getFileHandler()->getFileFromUrlProcessing(json["fileUrl"].toString(), "voiceFileData"));
        break;
    case 7:
    case 8: {
        QJsonObject voiceMessage = json;
        server->getFileHandler()->voiceMessageProcessing(voiceMessage);
        server->sendVoiceMessage(voiceMessage);
        break;
    }
    case 9: {
        QJsonObject createGroupJson = json;
        server->getFileHandler()->createGroupWithAvatarProcessing(createGroupJson);
        break;
    }
    case 10:
        setIdentifiers(json["userlogin"].toString(),json["user_id"].toInt());
        break;
    default:
        logger.log(Logger::WARN,"fileclienthandler.cpp::processClientRequest", "Unknown flag: " + json["flag"].toString());
        break;
    }
}

void FileClientHandler::processSendQueue()
{
    QMutexLocker lock(&mutex);
    if (sendQueue.isEmpty()) {
        logger.log(Logger::DEBUG, "fileclienthandler.cpp::processSendQueue", "Send queue is empty. Nothing to process.");
        return;
    }

    QByteArray jsonData = sendQueue.head();
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());
    logger.log(Logger::INFO, "fileclienthandler.cpp::processSendQueue", "Preparing to send data. Data size: " + QString::number(jsonData.size()) + " bytes");

    if (fileSocket->state() != QAbstractSocket::ConnectedState) {
        logger.log(Logger::DEBUG, "fileclienthandler.cpp::processSendQueue", "Socket is not connected. Cannot send data.");
        return;
    }
    if (fileSocket->write(bytes) == -1) {
        logger.log(Logger::DEBUG, "fileclienthandler.cpp::processSendQueue", "Failed to write data: " + fileSocket->errorString());
        return;
    }
    logger.log(Logger::INFO, "fileclienthandler.cpp::processSendQueue", "Data written to socket. Data size: " + QString::number(bytes.size()) + " bytes");
}

const std::unordered_map<std::string_view, uint> FileClientHandler::flagMap = {
    {"avatarUrl", 1}, {"newAvatarData", 2},
    {"personal_file", 3}, {"group_file", 4},
    {"fileUrl", 5}, {"voiceFileUrl", 6},
    {"personal_voice_message", 7}, {"group_voice_message", 8},
    {"create_group", 9}, {"identifiers", 10}
};
