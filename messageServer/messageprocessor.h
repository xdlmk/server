#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <QObject>

#include <QJsonObject>
#include <QJsonDocument>

#include "clienthandler.h"

class MessageProcessor : public QObject {
    Q_OBJECT
public:
    explicit MessageProcessor(QObject *parent = nullptr);
    static void processMessage(const QByteArray &jsonData, ClientHandler *client);
    void updatingChatsProcess(QJsonObject json);
    void sendMessageToActiveSockets(QJsonObject json, int message_id, int dialog_id);

signals:
};

#endif // MESSAGEPROCESSOR_H
