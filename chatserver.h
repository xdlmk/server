#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>

#include <QList>
#include <QMultiMap>
#include <QSet>

#include <QJsonObject>
#include <QJsonDocument>

#include <QImage>
#include <QBuffer>
#include <QImageReader>

#include <QSqlDatabase>
#include <QtSql>
#include <QSqlQuery>

#include <QCryptographicHash>

class ChatServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);

signals:

    // QTcpServer interface
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;

public slots:
    void saveFileToDatabase(const QString& fileUrl);
    void setAvatarInDatabase(const QString& avatarUrl, const int& user_id);
private slots:
    void readClient();
    void disconnectClient();
    void SendToClient(QJsonDocument doc,const QString& senderLogin);

private:
    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    void sendJson(const QJsonDocument &sendDoc);

    QJsonObject loginProcess(QJsonObject json);
    QJsonObject regProcess(QJsonObject json);
    QJsonObject searchProcess(QJsonObject json);
    QJsonObject editProfileProcess(QJsonObject dataEditProfile);

    void updatingChatsProcess(QJsonObject json);
    void personalMessageProcess(QJsonObject json);

    QString getAvatarUrl(const QString& userlogin);
    int getOrCreateDialog(int sender_id, int receiver_id);
    int saveMessageToDatabase(int dialogId, int senderId, int receiverId, const QString &message, const QString& fileUrl = "");
    void sendMessageToActiveSockets(QJsonObject json, int message_id, int dialog_id);

    void connectToDB();

    QTcpSocket *socket;
    QByteArray data;
    QMultiMap <QString, QTcpSocket*> clients;
};

#endif // CHATSERVER_H
