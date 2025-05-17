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

        QByteArray envelopeData;
        envelopeData.resize(blockSize);
        in.readRawData(envelopeData.data(), blockSize);

        QProtobufSerializer serializer;
        messages::Envelope envelope;
        if (!envelope.deserialize(&serializer, envelopeData)) {
            logger.log(Logger::INFO, "clienthandler.cpp::readClient", "Failed to deserialize protobuf");
            blockSize = 0;
            return;
        }

        QString flag = envelope.flag();
        QByteArray payload = envelope.payload();

        handleFlag(flag, payload);

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

void ClientHandler::sendData(const QString &flag, const QByteArray &data)
{
    messages::Envelope envelope;
    envelope.setFlag(flag);
    envelope.setPayload(data);
    QProtobufSerializer serializer;
    QByteArray envelopeData = envelope.serialize(&serializer);

    logger.log(
        Logger::INFO,
        "messagenetworkmanager.cpp::sendData",
        "Sending envelope for flag: " + flag
        );

    bool shouldStartProcessing = false;
    {
        QMutexLocker lock(&mutex);
        if (sendQueue.size() >= MAX_QUEUE_SIZE) {
            logger.log(
                Logger::DEBUG, "clienthandler.cpp::sendData",
                "Send queue overflow! Dropping message."
                );
            return;
        }
        sendQueue.enqueue(QSharedPointer<QByteArray>::create(envelopeData));
        logger.log(
            Logger::INFO, "clienthandler.cpp::sendData",
            "Message added to queue. Queue size: " + QString::number(sendQueue.size())
            );
        shouldStartProcessing = sendQueue.size() == 1;
    }

    if (shouldStartProcessing) {
        logger.log(
            Logger::INFO,
            "clienthandler.cpp::sendData",
            "Starting to process the send queue."
            );
        processSendQueue();
    }
}

void ClientHandler::handleFlag(const QString &flag, const QByteArray &data)
{
    logger.log(Logger::INFO, "clienthandler.cpp::handleFlag", "Get message for " + flag);
    auto it = flagMap.find(flag.toStdString());
    uint flagId = (it != flagMap.end()) ? it->second : 0;

    auto& db = DatabaseConnector::instance();
    switch (flagId) {
    case 1:
        sendData("login", db.getUserManager()->loginUser(data));
        break;
    case 2:
        sendData("reg",db.getUserManager()->registerUser(data));
        break;
    case 3: break;
    case 4:
        sendData("search",db.getUserManager()->searchUsers(data));
        break;
    case 5: {
        QString r = MessageProcessor::personalMessageProcess(data, manager);
        if(r != QString("t")) {
            logger.log(Logger::DEBUG,"clienthandler.cpp::handleFlag", "Error personal message process: " + r);
            chats::ErrorSendMessage errorSendMessage;
            errorSendMessage.setErrorType(r);
            errorSendMessage.setChatMessage(data);
            QProtobufSerializer serializer;
            sendData("personal_message_error", errorSendMessage.serialize(&serializer));
        }
        break;
    }
    case 6:
        MessageProcessor::groupMessageProcess(data, manager);
        break;
    case 7:{
        chats::UpdatingChatsRequest request;
        QProtobufSerializer serializer;
        request.deserialize(&serializer,data);
        sendData("updating_chats", db.getChatManager()->updatingChatsProcess(request.userId()).serialize(&serializer));
        break;
    }
    case 8: {
        chats::ChatsInfoRequest request;
        QProtobufSerializer serializer;
        request.deserialize(&serializer,data);

        chats::ChatsInfoResponse response;
        response.setDialogsInfo(db.getChatManager()->getDialogInfo(request.userId()));
        response.setGroupsInfo(db.getGroupManager()->getGroupInfo(request.userId()));
        sendData("chats_info",response.serialize(&serializer));
        break;
    }
    case 9: {
        QProtobufSerializer serializer;
        groups::DeleteMemberRequest request;
        request.deserialize(&serializer, data);

        bool failed = true;
        QByteArray rmData = db.getGroupManager()->removeMemberFromGroup(request, failed);
        if(!failed){
            QList<int> members = db.getGroupManager()->getGroupMembers(request.groupId());
            MessageProcessor::sendGroupMessageToActiveSockets(rmData, "delete_member", members, manager);
        } else {
            sendData("delete_member",rmData);
        }
        break;
    }
    case 10: {
        QProtobufSerializer serializer;
        groups::AddGroupMembersRequest request;
        request.deserialize(&serializer, data);

        QByteArray amData = db.getGroupManager()->addMemberToGroup(request);
        QList<int> members = db.getGroupManager()->getGroupMembers(request.groupId());
        MessageProcessor::sendGroupMessageToActiveSockets(amData, "add_group_members", members, manager);
        break;
    }
    case 11:
        sendData("load_messages",db.getChatManager()->loadMessages(data));
        break;
    case 12:
        sendData("edit", db.getUserManager()->editUserProfile(data));
        break;
    case 13:
        sendData("avatars_update", db.getUserManager()->getCurrentAvatarUrlById(data));
        break;
    case 14:
        db.getGroupManager()->createGroup(data, manager);
        break;
    case 15:{
        common::Identifiers ident;
        QProtobufSerializer serializer;
        ident.deserialize(&serializer,data);
        setIdentifiers(ident.userId());
        break;
    }
    case 16:
        sendData("create_dialog", db.getChatManager()->getDataForCreateDialog(data));
        break;
    case 17:{
        chats::MarkMessageRequest request;
        QProtobufSerializer serializer;
        request.deserialize(&serializer, data);
        if(db.getChatManager()->markMessage(request.messageId(), request.readerId())) {
            chats::MarkMessageResponse response;
            response.setMessageId(request.messageId());
            response.setReaderId(request.readerId());
            QPair<quint64, quint64> ids = db.getChatManager()->getSenderReceiverByMessageId(request.messageId());
            if(ids == qMakePair(0ULL,0ULL)) break;
            response.setChatId(request.readerId() == ids.first ? ids.second : ids.first);
            MessageProcessor::sendMessageToActiveSockets(response.serialize(&serializer), "mark_message", ids.first, ids.second, manager);
        }
        break;
    }
    case 18:
        sendData("create_dialog_with_keys", db.getChatManager()->createDialog(data));
        break;
    default:
        logger.log(Logger::INFO,"clienthandler.cpp::handleFlag", "Unknown flag: " + flag);
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
    {"identifiers", 15}, {"create_dialog", 16},
    {"mark_message", 17}, {"create_dialog_with_keys", 18}
};
