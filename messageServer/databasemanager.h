#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QCryptographicHash>

#include <QJsonObject>
#include <QJsonArray>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    void connectToDB();

    QJsonObject loginProcess(QJsonObject json);
    QJsonObject regProcess(QJsonObject json);
    QJsonObject searchProcess(QJsonObject json);
    QJsonObject editProfileProcess(QJsonObject json);

    void saveFileToDatabase(const QString &fileUrl);

    void setAvatarInDatabase(const QString &avatarUrl, const int &user_id);
    QString getAvatarUrl(const int& user_id);
    void getCurrentAvatarUrlById(const QJsonArray &idsArray);

    int getOrCreateDialog(int sender_id, int receiver_id);
    int saveMessageToDatabase(int dialogId, int senderId, int receiverId, const QString &message, const QString& fileUrl = "");
private:
    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    DatabaseManager() {}
};

#endif // DATABASEMANAGER_H
