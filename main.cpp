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
    QObject::connect(&fileServer, &FileServer::sendVoiceMessage,[&messageServer](QJsonObject json){
        messageServer.personalMessageProcess(json,&messageServer);
    });
    return a.exec();
}
