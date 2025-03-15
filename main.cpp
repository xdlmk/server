#include <QCoreApplication>
#include "fileServer/fileserver.h"
#include "messageServer/chatnetworkmanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ChatNetworkManager messageServer;
    FileServer fileServer;

    QObject::connect(&fileServer, &FileServer::saveFileToDatabase, &messageServer, &ChatNetworkManager::saveFileToDatabase);
    QObject::connect(&fileServer, &FileServer::setAvatarInDatabase, &messageServer, &ChatNetworkManager::setAvatarInDatabase);
    QObject::connect(&fileServer, &FileServer::setGroupAvatarInDatabase, &messageServer, &ChatNetworkManager::setGroupAvatarInDatabase);
    QObject::connect(&fileServer, &FileServer::sendVoiceMessage,[&messageServer](QJsonObject json){
        messageServer.personalMessageProcess(json,&messageServer);
    });
    QObject::connect(&fileServer, &FileServer::sendNewGroupAvatarUrlToActiveSockets,[&messageServer](const QJsonObject& json){
        messageServer.sendNewGroupAvatarUrlToActiveSockets(json,&messageServer);
    });
    QObject::connect(&fileServer, &FileServer::createGroup,[&messageServer](QJsonObject json){
        messageServer.createGroup(json);
    });
    return a.exec();
}
