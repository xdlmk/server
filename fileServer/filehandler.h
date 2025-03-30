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

class FileHandler : public QObject {
    Q_OBJECT
public:
    explicit FileHandler(QObject *parent = nullptr);

    QJsonObject getAvatarFromServer(const QJsonObject &json);
    QJsonObject makeAvatarUrlProcessing(const QJsonObject &json);
    QString makeUrlProcessing(const QJsonObject &json);
    QJsonObject getFileFromUrlProcessing(const QString &fileUrl, const QString &flag);
    void voiceMessageProcessing(QJsonObject &voiceJson);
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
