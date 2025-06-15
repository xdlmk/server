#include "CommandInterfaceServer.h"
#include <QDebug>
#include <QCoreApplication>
#include <QtGlobal>
#include <QTimeZone>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif

#include "../messageServer/Database/databaseconnector.h"
#include "../messageServer/Database/usermanager.h"
#include "../messageServer/Database/groupmanager.h"
#include "../messageServer/Database/chatmanager.h"

QElapsedTimer CommandInterfaceServer::uptimeTimer;

CommandInterfaceServer::CommandInterfaceServer(QObject* parent)
    : QObject(parent), logger(Logger::instance())
{
    uptimeTimer.start();
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

void CommandInterfaceServer::handleCommand(const QString& cmd, QLocalSocket* socket)
{
    if (cmd == "stop") {
        socket->write("OK\n");
        socket->flush();
        socket->disconnectFromServer();
        socket->deleteLater();
        QCoreApplication::quit();
        return;
    } else if (cmd.startsWith("delete-user ")) {
        quint64 userId = cmd.section(' ', 1).toULongLong();
        try{
            DatabaseConnector::instance().getUserManager()->deleteUser(userId);
            socket->write("User deleted\n");
        } catch (const std::exception &e) {
            QByteArray errMsg = "Error deleting user: ";
            errMsg += e.what();
            errMsg += "\n";
            socket->write(errMsg);
        }
    } else if (cmd.startsWith("delete-group ")) {
        quint64 groupId = cmd.section(' ', 1).toULongLong();
        try{
            DatabaseConnector::instance().getGroupManager()->deleteGroup(groupId);
            socket->write("Group deleted\n");
        } catch (const std::exception &e) {
            QByteArray errMsg = "Error deleting group: ";
            errMsg += e.what();
            errMsg += "\n";
            socket->write(errMsg);
        }
    }
    else if (cmd.startsWith("delete-message ")) {
        quint64 msgId = cmd.section(' ', 1).toULongLong();
        try{
            DatabaseConnector::instance().getChatManager()->deleteMessage(msgId);
            socket->write("Message deleted\n");
        } catch (const std::exception &e) {
            QByteArray errMsg = "Error deleting message: ";
            errMsg += e.what();
            errMsg += "\n";
            socket->write(errMsg);
        }
    }
    else if (cmd.startsWith("delete-file ")) {
        quint64 fileId = cmd.section(' ', 1).toULongLong();
        try{
            DatabaseConnector::instance().getFileManager()->deleteFile(fileId);
            socket->write("File deleted\n");
        } catch (const std::exception &e) {
            QByteArray errMsg = "Error deleting file: ";
            errMsg += e.what();
            errMsg += "\n";
            socket->write(errMsg);
        }
    }
    else if (cmd.startsWith("group-members ")) {
        quint64 groupId = cmd.section(' ', 1).toULongLong();
        try {
            QList<quint64> members = DatabaseConnector::instance().getGroupManager()->getGroupMembers(groupId);

            if (members.isEmpty()) {
                socket->write("No members found in the group.\n");
            } else {
                QStringList memberStrings;
                for (quint64 id : members) {
                    memberStrings << QString::number(id);
                }

                QString result = QString("Group Members [%1]: %2\n")
                                     .arg(members.size())
                                     .arg(memberStrings.join(", "));

                socket->write(result.toUtf8());
            }
        } catch (const std::exception &e) {
            QString errorMessage = QString("Failed to retrieve group members: %1\n").arg(e.what());
            socket->write(errorMessage.toUtf8());
        }
    }
    else if (cmd == "status") {
        QString status;
        status += "Server is running\n";
        status += "Database status: " + QString(databaseStatus() ? "ok\n" : "not working\n");
        status += QString("Uptime: %1\n").arg(QDateTime::fromSecsSinceEpoch(uptimeTimer.elapsed() / 1000, QTimeZone::UTC).toString("hh:mm:ss"));

        qint64 kilobytes = getMemoryUsageInKB();
        QString memoryUsage;
        if (kilobytes < 1024) {
            memoryUsage = QString("%1 KB").arg(kilobytes);
        } else if (kilobytes < 1024 * 1024) {
            double megabytes = kilobytes / 1024.0;
            memoryUsage = QString::number(megabytes, 'f', 1) + " MB";
        } else {
            double gigabytes = kilobytes / (1024.0 * 1024.0);
            memoryUsage = QString::number(gigabytes, 'f', 1) + " GB";
        }

        status += QString("Memory usage: %1\n").arg(memoryUsage);
        socket->write(status.toUtf8());

    } else {
        socket->write("Unknown command\n");
    }

    socket->flush();
    socket->disconnectFromServer();
    socket->deleteLater();
}

bool CommandInterfaceServer::databaseStatus()
{
    QSqlQuery query;
    if (!DatabaseConnector::instance().executeQuery(query, "SELECT 1;")) {
        return false;
    }
    return true;
}

qint64 CommandInterfaceServer::getMemoryUsageInKB()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return counters.WorkingSetSize / 1024;
    }
    return -1;
#elif defined(Q_OS_UNIX)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef Q_OS_MAC
        return usage.ru_maxrss / 1024;
#else
        return usage.ru_maxrss;
#endif
    }
    return -1;
#else
    return -1;
#endif
}

