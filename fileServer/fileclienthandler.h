#ifndef FILECLIENTHANDLER_H
#define FILECLIENTHANDLER_H

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

#include "fileserver.h"

#include "../Utils/logger.h"

#include "generated_protobuf/envelope.qpb.h"
#include "generated_protobuf/identifiers.qpb.h"
#include "QProtobufSerializer"
class FileClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit FileClientHandler(quintptr handle, FileServer *server, QObject *parent = nullptr);

    bool setIdentifiers(const int& id);
private slots:
    void readClient();
    void handleBytesWritten(qint64 bytes);
    void disconnectClient();

    void sendData(const QString &flag, const QByteArray &data);
signals:
    void clientDisconnected(FileClientHandler *client);
private:
    void processClientRequest(const QString &flag, const QByteArray &data);
    void processSendQueue();

    int id;
    QTcpSocket *fileSocket;
    QByteArray writeBuffer;
    quint32 blockSize = 0;
    QQueue<QSharedPointer<QByteArray>> sendQueue;
    QMutex mutex;

    FileServer* server;
    Logger& logger;

    const int MAX_QUEUE_SIZE = 1000;
    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // FILECLIENTHANDLER_H
