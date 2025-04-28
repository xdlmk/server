#include "chatnetworkmanager.h"

#include "clienthandler.h"

#include "Database/databaseconnector.h"
#include "Database/usermanager.h"
#include "Database/chatmanager.h"
#include "Database/groupmanager.h"
#include "Database/filemanager.h"

ChatNetworkManager::ChatNetworkManager(QObject *parent) : QTcpServer(parent), logger(Logger::instance()) {
    if(!this->listen(QHostAddress::Any,2020)) {
        logger.log(Logger::FATAL,"chatnetoworkmanager.cpp::constructor", "Unable to start the server: " + this->errorString());
    } else {
        logger.log(Logger::INFO,"chatnetoworkmanager.cpp::constructor", "Server started and listening adresses: " + this->serverAddress().toString() + " on port: " + QString::number(this->serverPort()));
        DatabaseConnector::instance();

        QObject::connect(this,&ChatNetworkManager::saveFileToDatabase,DatabaseConnector::instance().getFileManager(),&FileManager::saveFileRecord);
        QObject::connect(this,&ChatNetworkManager::setAvatarInDatabase,DatabaseConnector::instance().getUserManager().get(),&UserManager::setUserAvatar);
        QObject::connect(this,&ChatNetworkManager::setGroupAvatarInDatabase,DatabaseConnector::instance().getGroupManager().get(),&GroupManager::setGroupAvatar);

        QObject::connect(this,&ChatNetworkManager::personalMessageProcess,[](const QByteArray &data, ChatNetworkManager *manager) {
            MessageProcessor::personalMessageProcess(data, manager);
        });
        QObject::connect(this,&ChatNetworkManager::groupMessageProcess,[](const QByteArray &data, ChatNetworkManager *manager) {
            MessageProcessor::groupMessageProcess(data, manager);
        });
        QObject::connect(this, &ChatNetworkManager::createGroup,[this](const QByteArray& data){
            DatabaseConnector::instance().getGroupManager()->createGroup(data, this);
        });
    }
}

QList<ClientHandler*> ChatNetworkManager::getClients()
{
    return clients;
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
    logger.log(Logger::INFO,"chatnetoworkmanager.cpp::removeClient", "Client disconnected and removed.");
}
