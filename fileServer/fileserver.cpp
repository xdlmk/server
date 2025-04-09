#include "fileserver.h"
#include "fileclienthandler.h"

FileServer::FileServer(QObject *parent) : QTcpServer{parent}, logger(Logger::instance()), fileHandler(new FileHandler(this)) {
    if(!this->listen(QHostAddress::Any,2021)) {
        logger.log(Logger::FATAL,"fileserver.cpp::constructor", "Unable to start the FileServer: " + this->errorString());
    } else {
        logger.log(Logger::INFO,"fileserver.cpp::constructor", "FileServer started and listening adresses: " + this->serverAddress().toString() + " on port: " + QString::number(this->serverPort()));
    }
    connect(fileHandler,&FileHandler::saveFileToDatabase,this,&FileServer::saveFileToDatabase);
    connect(fileHandler,&FileHandler::setAvatarInDatabase,this,&FileServer::setAvatarInDatabase);
    connect(fileHandler,&FileHandler::setGroupAvatarInDatabase,this,&FileServer::setGroupAvatarInDatabase);
    connect(fileHandler,&FileHandler::createGroup,this,&FileServer::createGroup);
}

FileHandler *FileServer::getFileHandler()
{
    return fileHandler;
}

void FileServer::incomingConnection(qintptr handle) {
    FileClientHandler *client = new FileClientHandler(handle, this, this);
    connect(client, &FileClientHandler::clientDisconnected, this, &FileServer::removeClient);
    clients.append(client);
}

void FileServer::removeClient(FileClientHandler *client)
{
    clients.removeAll(client);
    client->deleteLater();
    logger.log(Logger::INFO,"fileserver.cpp::removeClient", "Client disconnected and removed.");
}
