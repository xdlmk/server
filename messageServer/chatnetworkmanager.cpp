#include "chatnetworkmanager.h"
#include "clienthandler.h"

ChatNetworkManager::ChatNetworkManager(QObject *parent) : QTcpServer(parent) {
    if(!this->listen(QHostAddress::Any,2020)) {
        qDebug() <<"Unable to start the server: " << this->errorString();
    } else {
        qDebug() << "Server started on host " << this->serverAddress().toString() <<" port " <<this->serverPort();
        DatabaseManager::instance().connectToDB();

        QObject::connect(this,&ChatNetworkManager::saveFileToDatabase,&DatabaseManager::instance(),&DatabaseManager::saveFileToDatabase);
        QObject::connect(this,&ChatNetworkManager::setAvatarInDatabase,&DatabaseManager::instance(),&DatabaseManager::setAvatarInDatabase);
        QObject::connect(this,&ChatNetworkManager::personalMessageProcess,[](QJsonObject &json, ChatNetworkManager *manager) {
            MessageProcessor::personalMessageProcess(json, manager);
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
