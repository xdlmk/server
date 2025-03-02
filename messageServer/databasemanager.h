#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QCryptographicHash>

#include <QJsonObject>
#include <QJsonArray>

#include <QTcpSocket>

class ChatNetworkManager;

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance(QObject *parent = nullptr);
    explicit DatabaseManager(QObject *parent = nullptr);
    void connectToDB();

    QJsonObject loginProcess(QJsonObject json, ChatNetworkManager *manager, QTcpSocket *socket);
    QJsonObject regProcess(QJsonObject json);
    QJsonObject searchProcess(QJsonObject json);
    QJsonObject editProfileProcess(QJsonObject json);
    QJsonObject getCurrentAvatarUrlById(const QJsonArray &idsArray);
    QJsonObject updatingChatsProcess(QJsonObject json);

    void saveFileToDatabase(const QString &fileUrl);

    void setAvatarInDatabase(const QString &avatarUrl, const int &user_id);
    QString getAvatarUrl(const int& user_id);

    QList<int> getGroupMembers(const int& group_id);
    void createGroup(QJsonObject json);

    int getOrCreateDialog(int sender_id, int receiver_id);
    int saveMessageToDatabase(int dialogId, int senderId, int receiverId, const QString &message, const QString& fileUrl, const QString& flag);
signals:
    void setIdentifiersForClient(QTcpSocket *socket, const QString &login, const int &id);

private:
    void processChatHistory(const QJsonArray &chatHistory, QJsonArray &jsonMessageArray);
    void processUserLogin(QJsonObject json, QJsonArray &jsonMessageArray);
    int getUserId(const QString &userlogin);
    QList<int> getUserDialogs(int user_id);
    void filterDialogs(QList<int> &dialogIds, const QJsonArray &dialogIdsArray);
    void appendMessageObject(QSqlQuery &query, QJsonArray &jsonMessageArray);
    QString getUserLogin(int user_id);

    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    DatabaseManager() {}
};

#endif // DATABASEMANAGER_H
