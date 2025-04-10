#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>
#include <QTcpServer>
#include <QJsonObject>

#include "FileHandler.h"
#include "../Utils/logger.h"

class FileClientHandler;
class FileServer : public QTcpServer {
    Q_OBJECT
public:
    explicit FileServer(QObject *parent = nullptr);

    FileHandler *getFileHandler();
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
private slots:
    void removeClient(FileClientHandler *client);
signals:
    void sendFileMessage(QJsonObject fileJson);
    void sendVoiceMessage(QJsonObject voiceJson);

    void setAvatarInDatabase(const QString& avatarUrl, const int& user_id);
    void setGroupAvatarInDatabase(const QString& avatarUrl, const int& group_id);
    void saveFileToDatabase(const QString& fileUrl);
    void createGroup(const QJsonObject& createGroupJson);
    void sendNewGroupAvatarUrlToActiveSockets(const QJsonObject& json);
private:
    FileHandler* fileHandler;

    QList<FileClientHandler*> clients;
    Logger& logger;
};

#endif // FILESERVER_H
