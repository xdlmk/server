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
        logger.log(Logger::INFO,"clienthandler.cpp::constructor", "Client connect: " + handle);
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

bool ClientHandler::setIdentifiers(const QString &login, const int &id)
{
    this->login = login;
    this->id = id;
    return !this->login.isEmpty() && this->id > 0;
}

QString ClientHandler::getLogin()
{
    return login;
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

    quint32 blockSize = 0;
    if (blockSize == 0 && socket->bytesAvailable() < sizeof(quint32)) return;
    if (blockSize == 0) in >> blockSize;
    if (socket->bytesAvailable() < blockSize) return;

    QByteArray jsonData;
    jsonData.resize(blockSize);
    in.readRawData(jsonData.data(), blockSize);

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) {
        blockSize = 0;
        return;
    }

    QJsonObject json = doc.object();
    QString flag = json["flag"].toString();

    handleFlag(flag,json,socket);


    blockSize = 0;
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
    logger.log(Logger::INFO,"clienthandler.cpp::sendJson", "JSON to send (compact):" + sendDoc.toJson(QJsonDocument::Compact));
    QByteArray jsonData = sendDoc.toJson(QJsonDocument::Compact);

    QByteArray bytes;
    bytes.clear();
    QDataStream out(&bytes,QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_7);

    out << quint32(jsonData.size());
    out.writeRawData(jsonData.data(), jsonData.size());

    socket->write(bytes);
    socket->flush();
    socket->waitForBytesWritten(3000);
}

void ClientHandler::handleFlag(const QString &flag, QJsonObject &json, QTcpSocket *socket)
{
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
    default:
        logger.log(Logger::WARN,"clienthandler.cpp::handleFlag", "Unknown flag: " + flag);
        break;
    }
}

const std::unordered_map<std::string_view, uint> ClientHandler::flagMap = {
    {"login", 1}, {"reg", 2}, {"logout", 3}, {"search", 4},
    {"personal_message", 5}, {"group_message", 6},
    {"updating_chats", 7}, {"chats_info", 8},
    {"delete_member", 9}, {"add_group_members", 10},
    {"load_messages", 11}, {"edit", 12},
    {"avatars_update", 13}, {"create_group", 14}
};
