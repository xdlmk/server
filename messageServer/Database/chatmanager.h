#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>

#include <QJsonObject>
#include <QJsonArray>

#include <QSqlQuery>

#include "groupmanager.h"

#include "../../Utils/logger.h"

#include "generated_protobuf/chatsInfo.qpb.h"
#include "generated_protobuf/updatingChats.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class DatabaseConnector;

class ChatManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QList<messages::DialogInfoItem> getDialogInfo(const int &user_id);
    int getOrCreateDialog(int sender_id, int receiver_id);

    chats::UpdatingChatsResponse updatingChatsProcess(const quint64 &user_id);

    int saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &flag);
    QJsonObject loadMessages(const QJsonObject &requestJson);

private:
    QList<chats::ChatMessage> getUserMessages(const quint64 user_id, bool &failed);
    QList<int> getUserDialogs(int user_id);
    chats::ChatMessage generatePersonalMessageObject(QSqlQuery &query);
    chats::ChatMessage generateGroupMessageObject(QSqlQuery &query);

    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // CHATMANAGER_H
