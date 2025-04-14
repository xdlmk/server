#include "clienthandler.h"

#include "Database/databaseconnector.h"
#include "Database/chatmanager.h"
#include "Database/usermanager.h"
#include "Database/groupmanager.h"

ClientHandler::ClientHandler(quintptr handle, ChatNetworkManager *manager, QObject *parent)
    : QObject{parent}, socket(new QTcpSocket(this)), logger(Logger::instance()) {
    if(socket->setSocketDescriptor(handle)) {
        connect(socket, &QTcpSocket::readyRead, this, &ClientHandler::readClient);
        connect(socket, &QTcpSocket::disconnected, this, &ClientHandler::disconnectClient);
        connect(socket, &QTcpSocket::bytesWritten, this, &ClientHandler::handleBytesWritten);

        logger.log(Logger::INFO,"clienthandler.cpp::constructor", QString("Client connect: ") + QString::number(handle));
        this->manager = manager;
    } else {
        delete socket;
        emit clientDisconnected(this);
    }
}

bool ClientHandler::checkSocket(QTcpSocket *socket)
{
    return this->socket == socket;
}

bool ClientHandler::setIdentifiers(const int &id)
{
    logger.log(Logger::INFO,"clientHandler.cpp::setIdentifiers","Set id: " + QString::number(id));
    this->id = id;
    return this->id > 0;
}

int ClientHandler::getId()
{
    return id;
}

void ClientHandler::readClient()
{
    socket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_7);

    while (true) {
        if (blockSize == 0) {
            if (socket->bytesAvailable() < sizeof(quint32))
                return;
            in >> blockSize;
        }

        if (socket->bytesAvailable() < blockSize) return;

        /*QByteArray jsonData;
        jsonData.resize(blockSize);
        in.readRawData(jsonData.data(), blockSize);*/

        QByteArray envelopeData;
        envelopeData.resize(blockSize);
        in.readRawData(envelopeData.data(), blockSize);

        QProtobufSerializer serializer;
        messages::Envelope envelope;
        if (!envelope.deserialize(&serializer, envelopeData)) {
            logger.log(Logger::WARN, "clienthandler.cpp::readClient", "Failed to deserialize protobuf");
            blockSize = 0;
            return;
        }

        QString flag = envelope.flag();
        QByteArray payload = envelope.payload();


        /*QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isNull()) {
            blockSize = 0;
            return;
        }

        QJsonObject json = doc.object();
        QString flag = json["flag"].toString();

        handleFlag(flag,json,socket);*/

        blockSize = 0;
    }
}

void ClientHandler::handleBytesWritten(qint64 bytes)
{
    QMutexLocker lock(&mutex);

    logger.log(Logger::INFO, "clienthandler.cpp::handleBytesWritten", "Bytes written: " + QString::number(bytes));

    if (!sendQueue.isEmpty()) {
        sendQueue.dequeue();
        logger.log(Logger::INFO, "clienthandler.cpp::handleBytesWritten", "Message dequeued. Queue size: " + QString::number(sendQueue.size()));
        if (!sendQueue.isEmpty()) {
            logger.log(Logger::INFO, "clienthandler.cpp::handleBytesWritten", "More messages in queue. Scheduling next processing.");
            QTimer::singleShot(10, this, &ClientHandler::processSendQueue);
        }
    }
}

void ClientHandler::disconnectClient()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if(clientSocket) {
        emit clientDisconnected(this);
        clientSocket->deleteLater();
    }
}

void ClientHandler::sendJson(const QJsonObject &jsonToSend)
{
    QJsonDocument sendDoc(jsonToSend);
    if(jsonToSend["flag"].toString() != "updating_chats"){
        logger.log(Logger::INFO,"clienthandler.cpp::sendJson", "JSON to send (compact):" + sendDoc.toJson(QJsonDocument::Compact));
    } else {
        logger.log(Logger::INFO,"clienthandler.cpp::sendJson", "JSON to send (compact):" + jsonToSend["flag"].toString());
    }

    bool shouldStartProcessing = false;
    {
        QMutexLocker lock(&mutex);
        QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);
        if (sendQueue.size() >= MAX_QUEUE_SIZE) {
            logger.log(Logger::DEBUG, "clienthandler.cpp::sendJson", "Send queue overflow! Dropping message.");
            return;
        }
        sendQueue.enqueue(QSharedPointer<QByteArray>::create(jsonData));
        logger.log(Logger::INFO, "clienthandler.cpp::sendJson", "Message added to queue. Queue size: " + QString::number(sendQueue.size()));
        shouldStartProcessing = sendQueue.size() == 1;
    }

    if (shouldStartProcessing) {
        logger.log(Logger::INFO, "clienthandler.cpp::sendJson", "Starting to process the send queue.");
        processSendQueue();
    }
}

void ClientHandler::handleFlag(const QString &flag, QJsonObject &json, QTcpSocket *socket)
{
    logger.log(Logger::INFO, "clienthandler.cpp::handleFlag", "Get message for " + flag);
    auto it = flagMap.find(flag.toStdString());
    uint flagId = (it != flagMap.end()) ? it->second : 0;

    auto& db = DatabaseConnector::instance();
    switch (flagId) {
    case 1:
        sendJson(db.getUserManager()->loginUser(json, manager, socket));
        break;
    case 2:
        sendJson(db.getUserManager()->registerUser(json));
        break;
    case 3: break;
    case 4:
        sendJson(db.getUserManager()->searchUsers(json));
        break;
    case 5:
        MessageProcessor::personalMessageProcess(json, manager);
        break;
    case 6:
        MessageProcessor::groupMessageProcess(json, manager);
        break;
    case 7:
        sendJson(db.getChatManager()->updatingChatsProcess(json));
        break;
    case 8: {
        QJsonObject infoObject;
        infoObject["flag"] = "chats_info";
        infoObject["dialogs_info"] = db.getChatManager()->getDialogInfo(json);
        infoObject["groups_info"] = db.getGroupManager()->getGroupInfo(json);
        sendJson(infoObject);
        break;
    }
    case 9: {
        QJsonObject rmJson = db.getGroupManager()->removeMemberFromGroup(json);
        QList<int> members = db.getGroupManager()->getGroupMembers(json["group_id"].toInt());
        MessageProcessor::sendGroupMessageToActiveSockets(rmJson, manager, members);
        break;
    }
    case 10: {
        QJsonObject amJson = db.getGroupManager()->addMemberToGroup(json);
        QList<int> members = db.getGroupManager()->getGroupMembers(json["group_id"].toInt());
        MessageProcessor::sendGroupMessageToActiveSockets(amJson, manager, members);
        break;
    }
    case 11:
        sendJson(db.getChatManager()->loadMessages(json));
        break;
    case 12:
        sendJson(db.getUserManager()->editUserProfile(json));
        break;
    case 13:
        sendJson(db.getUserManager()->getCurrentAvatarUrlById(json));
        break;
    case 14:
        db.getGroupManager()->createGroup(json,manager);
        break;
    case 15:
        setIdentifiers(json["user_id"].toInt());
        break;
    default:
        logger.log(Logger::WARN,"clienthandler.cpp::handleFlag", "Unknown flag: " + flag);
        break;
    }
}

void ClientHandler::processSendQueue()
{
    QMutexLocker lock(&mutex);
    if (sendQueue.isEmpty()) {
        logger.log(Logger::DEBUG, "clienthandler.cpp::processSendQueue", "Send queue is empty. Nothing to process.");
        return;
    }

    QByteArray jsonData = *sendQueue.head();
    writeBuffer.clear();
    QDataStream out(&writeBuffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());
    logger.log(Logger::INFO, "clienthandler.cpp::processSendQueue", "Preparing to send data. Data size: " + QString::number(jsonData.size()) + " bytes");

    if (socket->state() != QAbstractSocket::ConnectedState) {
        logger.log(Logger::DEBUG, "clienthandler.cpp::processSendQueue", "Socket is not connected. Cannot send data.");
        return;
    }
    if (socket->write(writeBuffer) == -1) {
        logger.log(Logger::DEBUG, "clienthandler.cpp::processSendQueue", "Failed to write data: " + socket->errorString());
        return;
    }
    logger.log(Logger::INFO, "clienthandler.cpp::processSendQueue", "Data written to socket. Data size: " + QString::number(writeBuffer.size()) + " bytes");
}

const std::unordered_map<std::string_view, uint> ClientHandler::flagMap = {
    {"login", 1}, {"reg", 2}, {"logout", 3}, {"search", 4},
    {"personal_message", 5}, {"group_message", 6},
    {"updating_chats", 7}, {"chats_info", 8},
    {"delete_member", 9}, {"add_group_members", 10},
    {"load_messages", 11}, {"edit", 12},
    {"avatars_update", 13}, {"create_group", 14},
    {"identifiers", 15}
};
