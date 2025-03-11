#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>

#include <QTcpServer>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>
#include <QDataStream>

#include "FileHandler.h"

class FileServer : public QTcpServer {
    Q_OBJECT
public:
    explicit FileServer(QObject *parent = nullptr);
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
private slots:
    void readClient();
    void sendData(const QJsonObject &sendJson);
signals:
    void sendVoiceMessage(QJsonObject voiceJson);
    void setAvatarInDatabase(const QString& avatarUrl, const int& user_id);
    void saveFileToDatabase(const QString& fileUrl);
    void createGroup(const QJsonObject& createGroupJson);
private:
    void processClientRequest(const QJsonObject &json);
    QTcpSocket *socket;
    FileHandler fileHandler;
};

#endif // FILESERVER_H
