#include "clienthandler.h"

ClientHandler::ClientHandler(quintptr handle, ChatNetworkManager *manager, QObject *parent)
    : QObject{parent}, socket(new QTcpSocket(this)) {
    if(socket->setSocketDescriptor(handle)) {
        connect(socket, &QTcpSocket::readyRead, this, &ClientHandler::readClient);
        connect(socket, &QTcpSocket::disconnected, this, &ClientHandler::disconnectClient);
        qDebug() << "Client connect: " << handle;
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
        qDebug() << "Invalid JSON-message received";
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
    qDebug() << "JSON to send (compact):" << sendDoc.toJson(QJsonDocument::Compact);
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

    if(flag == "login") sendJson(DatabaseConnector::instance().getUserManager()->loginUser(json,manager,socket)); //sendJson(DatabaseManager::instance().loginProcess(json, manager, socket));
    else if(flag == "reg") sendJson(DatabaseConnector::instance().getUserManager()->registerUser(json)); // sendJson(DatabaseManager::instance().regProcess(json));
    else if(flag == "logout") ;
    else if(flag == "search") sendJson(DatabaseConnector::instance().getUserManager()->searchUsers(json)); //sendJson(DatabaseManager::instance().searchProcess(json));
    else if(flag == "personal_message") MessageProcessor::personalMessageProcess(json, manager);
    else if(flag == "group_message") MessageProcessor::groupMessageProcess(json, manager);
    else if(flag == "updating_chats") sendJson(DatabaseConnector::instance().getChatManager()->updatingChatsProcess(json)); //sendJson(DatabaseManager::instance().updatingChatsProcess(json));
    else if(flag == "chats_info") {
        QJsonObject infoObject;
        infoObject["flag"] = "chats_info";
        infoObject["dialogs_info"] = DatabaseConnector::instance().getChatManager()->getDialogInfo(json); // DatabaseManager::instance().getDialogsInformation(json);
        infoObject["groups_info"] = DatabaseConnector::instance().getGroupManager()->getGroupInfo(json); //DatabaseManager::instance().getGroupInformation(json);
        sendJson(infoObject);
    } else if(flag == "delete_member") MessageProcessor::sendGroupMessageToActiveSockets(DatabaseConnector::instance().getGroupManager()->removeMemberFromGroup(json), manager, DatabaseConnector::instance().getGroupManager()->getGroupMembers(json["group_id"].toInt())); //MessageProcessor::sendGroupMessageToActiveSockets(DatabaseManager::instance().deleteMemberFromGroup(json), manager, DatabaseManager::instance().getGroupMembers(json["group_id"].toInt()));
    else if(flag == "add_group_members") MessageProcessor::sendGroupMessageToActiveSockets(DatabaseConnector::instance().getGroupManager()->addMemberToGroup(json), manager, DatabaseConnector::instance().getGroupManager()->getGroupMembers(json["group_id"].toInt())); //MessageProcessor::sendGroupMessageToActiveSockets(DatabaseManager::instance().addMemberToGroup(json), manager, DatabaseManager::instance().getGroupMembers(json["group_id"].toInt()));
    else if(flag == "load_messages") sendJson(DatabaseConnector::instance().getChatManager()->loadMessages(json)); //sendJson(DatabaseManager::instance().loadMessagesProcess(json));
    else if(flag == "edit") sendJson(DatabaseConnector::instance().getUserManager()->editUserProfile(json)); //sendJson(DatabaseManager::instance().editProfileProcess(json));
    else if(flag == "avatars_update") sendJson(DatabaseConnector::instance().getUserManager()->getCurrentAvatarUrlById(json)); //sendJson(DatabaseManager::instance().getCurrentAvatarUrlById(json));
    else if(flag == "create_group") DatabaseConnector::instance().getGroupManager()->createGroup(json,manager); //DatabaseManager::instance().createGroup(json,manager);

}
