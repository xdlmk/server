#ifndef DATABASECONNECTOR_H
#define DATABASECONNECTOR_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

#include "usermanager.h"
#include "chatmanager.h"
#include "groupmanager.h"
#include "filemanager.h"

class DatabaseConnector : public QObject
{
    Q_OBJECT
public:
    static DatabaseConnector& instance(QObject *parent = nullptr);
    explicit DatabaseConnector(QObject *parent = nullptr);

    bool connectToDatabase();//
    bool executeQuery(QSqlQuery &query, const QString &queryStr, const QMap<QString, QVariant> &params = {});//

    UserManager *getUserManager();//
    ChatManager *getChatManager();//
    GroupManager *getGroupManager();//
    FileManager *getFileManager();//

private:
    UserManager userManager;
    ChatManager chatManager;
    GroupManager groupManager;
    FileManager fileManager;

    QSqlDatabase db;

signals:
};

#endif // DATABASECONNECTOR_H
