#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>

#include <QSqlQuery>

#include "groupmanager.h"

class DatabaseConnector;

class ChatManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QJsonObject getDialogInfo(const QJsonObject &json);  // DatabaseManager::getDialogsInformation
    int getOrCreateDialog(int sender_id, int receiver_id);  // DatabaseManager::getOrCreateDialog

    QJsonObject updatingChatsProcess(QJsonObject json);// DatabaseManager::updatingChatsProcess

    int saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &flag);  // DatabaseManager::saveMessageToDatabase
    QJsonObject loadMessages(const QJsonObject &requestJson);  // DatabaseManager::loadMessagesProcess

private:
    void getUserMessages(QJsonObject json, QJsonArray &jsonMessageArray);
    QList<int> getUserDialogs(int user_id);
    void appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray);  // DatabaseManager::appendMessageObject

    DatabaseConnector *databaseConnector;
};

#endif // CHATMANAGER_H
