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

bool FileClientHandler::setIdentifiers(const int &id)
{
    logger.log(Logger::INFO,"fileclientHandler.cpp::setIdentifiers","Set id: " + QString::number(id));
    this->id = id;
    return this->id > 0;
}

void FileClientHandler::readClient()
{
    logger.log(Logger::DEBUG,"fileclienthandler.cpp::readClient", "FileServer get message");
    fileSocket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(fileSocket);
    in.setVersion(QDataStream::Qt_6_7);

    while (true) {
        if (blockSize == 0) {
            if (fileSocket->bytesAvailable() < sizeof(quint32))
                return;
            in >> blockSize;
        }

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
    if(jsonToSend.contains("avatarData") || jsonToSend.contains("fileData")){
        QJsonObject logJson = jsonToSend;
        logJson["avatarData"] = logJson["fileData"] = "here data";
        logger.log(Logger::INFO,"fileclienthandler.cpp::sendData", "JSON to send (compact):" + QJsonDocument(logJson).toJson(QJsonDocument::Compact));
    } else {
        logger.log(Logger::INFO,"fileclienthandler.cpp::sendData", "JSON to send (compact):" + sendDoc.toJson(QJsonDocument::Compact));
    }
    bool shouldStartProcessing = false;
    {
        QMutexLocker lock(&mutex);
        QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);
        if (sendQueue.size() >= MAX_QUEUE_SIZE) {
            logger.log(Logger::DEBUG, "fileclienthandler.cpp::sendData", "Send queue overflow! Dropping message.");
            return;
        }
        sendQueue.enqueue(QSharedPointer<QByteArray>::create(jsonData));
        logger.log(Logger::INFO, "fileclienthandler.cpp::sendData", "Message added to queue. Queue size: " + QString::number(sendQueue.size()));
        shouldStartProcessing = sendQueue.size() == 1;
    }

    if (shouldStartProcessing) {
        logger.log(Logger::INFO, "fileclienthandler.cpp::sendData", "Starting to process the send queue.");
        processSendQueue();
    }
}

void FileClientHandler::processClientRequest(const QJsonObject &json)
{
    logger.log(Logger::INFO, "fileclienthandler.cpp::processClientRequest", "Get message for " + json["flag"].toString());
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
        sendData(server->getFileHandler()->getFileFromUrlProcessing(json["fileUrl"].toString(), "fileData"));
        break;
    case 4:
        sendData(server->getFileHandler()->getFileFromUrlProcessing(json["fileUrl"].toString(), "voiceFileData"));
        break;
    case 5:
    case 6: {
        QJsonObject voiceMessage = json;
        server->getFileHandler()->voiceMessageProcessing(voiceMessage);
        server->sendVoiceMessage(voiceMessage);
        break;
    }
    case 7:
    case 8:{
        QJsonObject fileMessage = json;
        server->getFileHandler()->fileMessageProcessing(fileMessage);
        server->sendFileMessage(fileMessage);
        break;
    }
    case 9: {
        QJsonObject createGroupJson = json;
        server->getFileHandler()->createGroupWithAvatarProcessing(createGroupJson);
        break;
    }
    case 10:
        setIdentifiers(json["user_id"].toInt());
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

    QByteArray jsonData = *sendQueue.head();
    writeBuffer.clear();
    QDataStream out(&writeBuffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());
    logger.log(Logger::INFO, "fileclienthandler.cpp::processSendQueue", "Preparing to send data. Data size: " + QString::number(jsonData.size()) + " bytes");

    if (fileSocket->state() != QAbstractSocket::ConnectedState) {
        logger.log(Logger::DEBUG, "fileclienthandler.cpp::processSendQueue", "Socket is not connected. Cannot send data.");
        return;
    }
    if (fileSocket->write(writeBuffer) == -1) {
        logger.log(Logger::DEBUG, "fileclienthandler.cpp::processSendQueue", "Failed to write data: " + fileSocket->errorString());
        return;
    }
    logger.log(Logger::INFO, "fileclienthandler.cpp::processSendQueue", "Data written to socket. Data size: " + QString::number(writeBuffer.size()) + " bytes");
}

const std::unordered_map<std::string_view, uint> FileClientHandler::flagMap = {
    {"avatarUrl", 1}, {"newAvatarData", 2},
    {"fileUrl", 3}, {"voiceFileUrl", 4},
    {"personal_voice_message", 5}, {"group_voice_message", 6},
    {"personal_file_message", 7}, {"group_file_message", 8},
    {"create_group", 9}, {"identifiers", 10}
};
