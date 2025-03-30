#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>

#include <QSqlQuery>

#include "groupmanager.h"

#include "../../Utils/logger.h"

class DatabaseConnector;

class ChatManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QJsonObject getDialogInfo(const QJsonObject &json);
    int getOrCreateDialog(int sender_id, int receiver_id);

    QJsonObject updatingChatsProcess(QJsonObject json);

    int saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &flag);
    QJsonObject loadMessages(const QJsonObject &requestJson);

private:
    void getUserMessages(QJsonObject json, QJsonArray &jsonMessageArray);
    QList<int> getUserDialogs(int user_id);
    void appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray);

    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // CHATMANAGER_H
