#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>

#include <QSqlQuery>
#include <QPair>

#include "groupmanager.h"

#include "../../Utils/logger.h"
#include "../../messageServer/messageprocessor.h"

#include "generated_protobuf/chatsInfo.qpb.h"
#include "generated_protobuf/updatingChats.qpb.h"
#include "generated_protobuf/loadMessages.qpb.h"
#include "generated_protobuf/chatMessage.qpb.h"
#include "generated_protobuf/markMessage.qpb.h"
#include "generated_protobuf/createDialog.qpb.h"
#include <QtProtobuf/qprotobufserializer.h>

class DatabaseConnector;

class ChatManager : public QObject
{
    Q_OBJECT
public:
    explicit ChatManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    QList<chats::DialogInfoItem> getDialogInfo(const int &user_id);

    QByteArray markMessage(const quint64 &message_id, const quint64 &reader_id);

    QPair<quint64, quint64> getSenderReceiverByMessageId(const quint64& messageId);
    quint64 getOrCreateDialog(int sender_id, int receiver_id, const QByteArray &sender_encrypted_session_key, const QByteArray &receiver_encrypted_session_key);
    quint64 getDialog(const quint64 &sender_id, const quint64 &receiver_id);

    QByteArray getEncryptedSessionKey(const quint64& dialog_id, const quint64& user_id);

    QByteArray getDataForCreateDialog(const QByteArray &requestData);

    chats::UpdatingChatsResponse updatingChatsProcess(const quint64 &user_id);

    quint64 saveMessage(int dialogId, int senderId, int receiverId, const QString &message, const QString &fileUrl, const QString &special_type, const QString &flag);
    QByteArray loadMessages(const QByteArray &requestData);

private:
    QList<chats::ChatMessage> getUserMessages(const quint64 user_id, bool &failed);
    QList<int> getUserDialogs(int user_id);
    chats::ChatMessage generatePersonalMessageObject(QSqlQuery &query);
    chats::ChatMessage generateGroupMessageObject(QSqlQuery &query);

    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // CHATMANAGER_H
