#ifndef COMMANDINTERFACESERVER_H
#define COMMANDINTERFACESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

#include "../Utils/logger.h"

class CommandInterfaceServer : public QObject
{
    Q_OBJECT

public:
    explicit CommandInterfaceServer(QObject* parent = nullptr);
    bool start(const QString& socketName);
    void stop();

private:
    QLocalServer* server = nullptr;

    void handleCommand(const QString& command, QLocalSocket* socket);

    Logger& logger;
};


#endif // COMMANDINTERFACESERVER_H
