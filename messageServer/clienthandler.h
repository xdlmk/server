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

#include "generated_protobuf/envelope.qpb.h"
#include "generated_protobuf/chatsInfo.qpb.h"
#include "generated_protobuf/deleteMember.qpb.h"
#include "generated_protobuf/addMembers.qpb.h"
#include "generated_protobuf/updatingChats.qpb.h"
#include "generated_protobuf/identifiers.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class ClientHandler : public QObject {
    Q_OBJECT
public:
    explicit ClientHandler(quintptr handle,ChatNetworkManager *manager, QObject *parent = nullptr);

    bool checkSocket(QTcpSocket *socket);
    bool setIdentifiers(const int& id);
    int getId();

    void sendData(const QString &flag, const QByteArray &data);
signals:
    void clientDisconnected(ClientHandler *client);
private slots:
    void readClient();
    void handleBytesWritten(qint64 bytes);
    void disconnectClient();
private:
    void handleFlag(const QString &flag, const QByteArray &data);

    void processSendQueue();

    int id;
    QTcpSocket *socket;
    QByteArray writeBuffer;
    quint32 blockSize = 0;
    QQueue<QSharedPointer<QByteArray>> sendQueue;
    QMutex mutex;

    ChatNetworkManager * manager;
    Logger& logger;

    const int MAX_QUEUE_SIZE = 1000;
    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // CLIENTHANDLER_H
