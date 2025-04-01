#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>

#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>

#include <unordered_map>
#include <string_view>
#include <QQueue>

#include "chatnetworkmanager.h"
#include "messageprocessor.h"

#include "../Utils/logger.h"

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(quintptr handle,ChatNetworkManager *manager, QObject *parent = nullptr);

    bool checkSocket(QTcpSocket *socket);
    bool setIdentifiers(const QString& login,const int& id);
    QString getLogin();
    int getId();

    void sendJson(const QJsonObject &jsonToSend);
signals:
    void clientDisconnected(ClientHandler *client);
private slots:
    void readClient();
    void handleBytesWritten(qint64 bytes);
    void disconnectClient();
private:
    void handleFlag(const QString &flag, QJsonObject &json, QTcpSocket *socket);
    void processSendQueue();

    QString login;
    int id;
    QTcpSocket *socket;
    QQueue<QByteArray> sendQueue;
    QMutex mutex;

    ChatNetworkManager * manager;
    Logger& logger;

    const int MAX_QUEUE_SIZE = 1000;
    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // CLIENTHANDLER_H
