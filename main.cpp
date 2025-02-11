#include <QCoreApplication>
#include "chatserver.h"
#include "fileserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ChatServer server;
    FileServer fileServer;

    QObject::connect(&fileServer, &FileServer::saveFileToDatabase, &server, &ChatServer::saveFileToDatabase);
    return a.exec();
}
