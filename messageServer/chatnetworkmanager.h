#ifndef CHATNETWORKMANAGER_H
#define CHATNETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include "clienthandler.h"
#include "databasemanager.h"

class ChatNetworkManager : public QTcpServer {
    Q_OBJECT
public:
    explicit ChatNetworkManager(QObject *parent = nullptr);
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
private:
    QList<ClientHandler*> clients;
    void removeClient(ClientHandler *client);
};

#endif // CHATNETWORKMANAGER_H
