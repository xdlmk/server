#ifndef DATABASECONNECTOR_H
#define DATABASECONNECTOR_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

#include "usermanager.h"
#include "filemanager.h"

class GroupManager;
class ChatManager;
class DatabaseConnector : public QObject
{
    Q_OBJECT
public:
    static DatabaseConnector& instance(QObject *parent = nullptr);
    explicit DatabaseConnector(QObject *parent = nullptr);

    bool connectToDatabase();//
    bool executeQuery(QSqlQuery &query, const QString &queryStr, const QMap<QString, QVariant> &params = {});//

    UserManager *getUserManager();//
    std::unique_ptr<ChatManager> &getChatManager();//
    FileManager *getFileManager();//
    std::unique_ptr<GroupManager> &getGroupManager();//

private:
    UserManager userManager;
    std::unique_ptr<ChatManager> chatManager;
    FileManager fileManager;
    std::unique_ptr<GroupManager> groupManager;

    QSqlDatabase db;

signals:
};

#endif // DATABASECONNECTOR_H
