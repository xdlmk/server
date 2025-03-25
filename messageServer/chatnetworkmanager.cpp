#include "chatnetworkmanager.h"
#include "clienthandler.h"

ChatNetworkManager::ChatNetworkManager(QObject *parent) : QTcpServer(parent) {
    if(!this->listen(QHostAddress::Any,2020)) {
        qDebug() <<"Unable to start the server: " << this->errorString();
    } else {
        qDebug() << "Server started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
        DatabaseConnector::instance();
        //DatabaseManager::instance().connectToDB();

        QObject::connect(this,&ChatNetworkManager::saveFileToDatabase,DatabaseConnector::instance().getFileManager(),&FileManager::saveFileRecord);
        QObject::connect(this,&ChatNetworkManager::setAvatarInDatabase,DatabaseConnector::instance().getUserManager(),&UserManager::setUserAvatar);
        QObject::connect(this,&ChatNetworkManager::setGroupAvatarInDatabase,DatabaseConnector::instance().getGroupManager(),&GroupManager::setGroupAvatar);

        QObject::connect(this,&ChatNetworkManager::personalMessageProcess,[](QJsonObject &json, ChatNetworkManager *manager) {
            MessageProcessor::personalMessageProcess(json, manager);
        });
        QObject::connect(this,&ChatNetworkManager::sendNewGroupAvatarUrlToActiveSockets,[](const QJsonObject &json, ChatNetworkManager *manager) {
            MessageProcessor::sendNewGroupAvatarUrlToActiveSockets(json, manager);
        });
        QObject::connect(this, &ChatNetworkManager::createGroup,[this](const QJsonObject& json){
            DatabaseConnector::instance().getGroupManager()->createGroup(json,this);
            //DatabaseManager::instance().createGroup(json,this);
        });
    }
}

QList<ClientHandler*> ChatNetworkManager::getClients()
{
    return clients;
}

void ChatNetworkManager::setIdentifiersForClient(QTcpSocket *socket, const QString &login, const int &id)
{
    for(ClientHandler* client : clients) {
        if(client->checkSocket(socket)) {
            client->setIdentifiers(login,id);
            qDebug() << "Set login: " + login + ", set id: " + QString::number(id);
            break;
        }
    }
}

void ChatNetworkManager::incomingConnection(qintptr handle)
{
    ClientHandler *client = new ClientHandler(handle, this, this);
    connect(client, &ClientHandler::clientDisconnected, this, &ChatNetworkManager::removeClient);
    clients.append(client);
}

void ChatNetworkManager::removeClient(ClientHandler *client)
{
    clients.removeAll(client);
    client->deleteLater();
    qDebug() << "Client disconnected and removed.";
}
