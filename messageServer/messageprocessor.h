#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <QObject>

#include <QJsonObject>
#include <QJsonDocument>
#include <QList>

#include "clienthandler.h"
#include "databasemanager.h"
#include "chatnetworkmanager.h"

class MessageProcessor : public QObject {
    Q_OBJECT
public:
    explicit MessageProcessor(QObject *parent = nullptr);
    static void personalMessageProcess(QJsonObject &json,ChatNetworkManager *manager);
    static void sendMessageToActiveSockets(QJsonObject json, ChatNetworkManager *manager, int message_id, int dialog_id);

private:
    static QJsonObject createMessageJson(QJsonObject json, int message_id, int dialog_id);
    static void sendToClient(ClientHandler *client, QJsonObject& messageJson, bool isSender);
};

#endif // MESSAGEPROCESSOR_H
