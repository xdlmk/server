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
        //messageServer.personalMessageProcess(json,&messageServer); change personalMessageProcess
    });
    QObject::connect(&fileServer, &FileServer::sendFileMessage,[&messageServer](QJsonObject json){
        //messageServer.personalMessageProcess(json,&messageServer); change personalMessageProcess
    });
    QObject::connect(&fileServer, &FileServer::createGroup,[&messageServer](const QByteArray &data){
        messageServer.createGroup(data);
    });
    return a.exec();
}
