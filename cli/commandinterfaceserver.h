#ifndef COMMANDINTERFACESERVER_H
#define COMMANDINTERFACESERVER_H

#include <QObject>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include "../messageServer/chatnetworkmanager.h"
#include "../fileServer/fileserver.h"
#include "../Utils/logger.h"

class CommandInterfaceServer : public QObject
{
    Q_OBJECT

public:
    explicit CommandInterfaceServer(ChatNetworkManager* serverManager, FileServer* fileServer, QObject* parent = nullptr);
    bool start(const QString& socketName);
    void stop();

private:
    QLocalServer* server = nullptr;

    void handleCommand(const QString& cmd, QLocalSocket* socket);
    bool databaseStatus();
    qint64 getMemoryUsageInKB();

    static QElapsedTimer uptimeTimer;

    ChatNetworkManager* serverManager;
    FileServer* fileServer;
    Logger& logger;
};


#endif // COMMANDINTERFACESERVER_H
