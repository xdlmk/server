#include <QCoreApplication>

#include "fileServer/fileserver.h"
#include "messageServer/chatnetworkmanager.h"
#include "messageServer/Database/databaseconnector.h"
#include "cli/commandinterfaceserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ChatNetworkManager messageServer;
    FileServer fileServer;
    CommandInterfaceServer cliServer(&messageServer);

    QObject::connect(&fileServer, &FileServer::saveFileToDatabase, &messageServer, &ChatNetworkManager::saveFileToDatabase);
    QObject::connect(&fileServer, &FileServer::setAvatarInDatabase, &messageServer, &ChatNetworkManager::setAvatarInDatabase);
    QObject::connect(&fileServer, &FileServer::setGroupAvatarInDatabase, &messageServer, &ChatNetworkManager::setGroupAvatarInDatabase);

    QObject::connect(&fileServer, &FileServer::sendVoiceMessage,[&messageServer](const QString& flag, const QByteArray &data){
        if(flag == "personal") {
            messageServer.personalMessageProcess(data, &messageServer);
        } else if(flag == "group") {
            messageServer.groupMessageProcess(data, &messageServer);
        }
    });
    QObject::connect(&fileServer, &FileServer::sendFileMessage,[&messageServer](const QString& flag, const QByteArray &data){
        if(flag == "personal") {
            messageServer.personalMessageProcess(data, &messageServer);
        } else if(flag == "group") {
            messageServer.groupMessageProcess(data, &messageServer);
        }
    });
    QObject::connect(&fileServer, &FileServer::createGroup,[&messageServer](const QByteArray &data){
        messageServer.createGroup(data);
    });
    return a.exec();
}
