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
class FileClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit FileClientHandler(quintptr handle, FileServer *server, QObject *parent = nullptr);

    bool setIdentifiers(const QString& login,const int& id);
private slots:
    void readClient();
    void handleBytesWritten(qint64 bytes);
    void disconnectClient();
    void sendData(const QJsonObject &jsonToSend);
signals:
    void clientDisconnected(FileClientHandler *client);
private:
    void processClientRequest(const QJsonObject &json);
    void processSendQueue();

    QString login;
    int id;
    QTcpSocket *fileSocket;
    QQueue<QByteArray> sendQueue;
    QMutex mutex;

    FileServer* server;
    Logger& logger;

    const int MAX_QUEUE_SIZE = 1000;
    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // FILECLIENTHANDLER_H
