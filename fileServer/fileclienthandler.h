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
    explicit FileClientHandler(quintptr handle, QObject *parent = nullptr);

private slots:
    void readClient();

private:
    void processClientRequest(const QJsonObject &json);

    QString login;
    int id;
    QTcpSocket *fileSocket;

    Logger& logger;

    static const std::unordered_map<std::string_view, uint> flagMap;
};

#endif // FILECLIENTHANDLER_H
