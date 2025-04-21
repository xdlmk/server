#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QObject>

#include <QDir>
#include <QFile>
#include <QUuid>

#include <QString>
#include <QJsonObject>
#include <QDebug>

#include "../Utils/logger.h"

#include "generated_protobuf/getAvatar.qpb.h"
#include <QProtobufSerializer>

class FileHandler : public QObject {
    Q_OBJECT
public:
    explicit FileHandler(QObject *parent = nullptr);

    QByteArray getAvatarFromServer(const QByteArray &data);
    QByteArray makeAvatarUrlProcessing(const QByteArray &data);
    QString makeUrlProcessing(const QString &fileName, const QString &fileExtension, const QByteArray &data);
    QJsonObject getFileFromUrlProcessing(const QString &fileUrl, const QString &flag);
    void voiceMessageProcessing(QJsonObject &voiceJson);
    void fileMessageProcessing(QJsonObject &fileJson);
    void createGroupWithAvatarProcessing(QJsonObject &createGroupJson);
signals:
    void setAvatarInDatabase(const QString& avatarUrl, const int& user_id);
    void setGroupAvatarInDatabase(const QString& avatarUrl, const int& user_id);
    void saveFileToDatabase(const QString& fileUrl);

    void createGroup(const QJsonObject& createGroupJson);

private:
    Logger& logger;
};

#endif // FILEHANDLER_H
