#include "CommandInterfaceServer.h"
#include <QDebug>
#include <QCoreApplication>

#include "../messageServer/Database/databaseconnector.h"
#include "../messageServer/Database/usermanager.h"

CommandInterfaceServer::CommandInterfaceServer(QObject* parent)
    : QObject(parent), logger(Logger::instance())
{
    server = new QLocalServer(this);

    connect(server, &QLocalServer::newConnection, this, [=]() {
        QLocalSocket* socket = server->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, socket, [=]() {
            QString command = QString::fromUtf8(socket->readAll()).trimmed();
            logger.log(Logger::INFO,"CommandInterfaceServer", "[CLI] Received command: " + command);
            handleCommand(command, socket);
        });
    });

    this->start("mserver_socket");
}

bool CommandInterfaceServer::start(const QString& socketName)
{
    QLocalServer::removeServer(socketName);
    if (!server->listen(socketName)) {
        logger.log(Logger::WARN,"CommandInterfaceServer.cpp::start", "Command server listen failed:" + server->errorString());
        return false;
    }
    logger.log(Logger::INFO,"CommandInterfaceServer.cpp::start", "[CLI] Command server listening on socket:" + socketName);
    return true;
}

void CommandInterfaceServer::stop()
{
    if (server->isListening())
        server->close();
}

void CommandInterfaceServer::handleCommand(const QString& command, QLocalSocket* socket)
{
    if (command == "stop") {
        socket->write("OK\n");
        socket->flush();
        socket->disconnectFromServer();
        socket->deleteLater();
        QCoreApplication::quit();
        return;
    }

    if (command.startsWith("delete-user ")) {
        QString userId = command.section(' ', 1);
        try{
            DatabaseConnector::instance().getUserManager()->deleteUser(userId.toULongLong());
            socket->write("User deleted\n");
        } catch (const std::exception &e) {
            QByteArray errMsg = "Error deleting user: ";
            errMsg += e.what();
            errMsg += "\n";
            socket->write(errMsg);
        }
    } else {
        socket->write("Unknown command\n");
    }

    socket->flush();
    socket->disconnectFromServer();
    socket->deleteLater();
}

