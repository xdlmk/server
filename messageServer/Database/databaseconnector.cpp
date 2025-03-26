#include "databaseconnector.h"
#include "groupmanager.h"
#include "chatmanager.h"

DatabaseConnector &DatabaseConnector::instance(QObject *parent) {
    static DatabaseConnector instance(parent);
    return instance;
}

DatabaseConnector::DatabaseConnector(QObject *parent)
    : QObject{parent}, userManager(this),
    fileManager(this)
{
    groupManager = std::make_unique<GroupManager>(this);
    chatManager = std::make_unique<ChatManager>(this);
    while(!connectToDatabase()) {
        qDebug() << "Connect to database failed";
    }
    qDebug() << "Database connected";
}

bool DatabaseConnector::connectToDatabase()
{
    db = QSqlDatabase::addDatabase("QMARIADB");
    db.setHostName("localhost");
    db.setPort(3306);
    db.setDatabaseName("test_db");
    db.setUserName("admin");
    db.setPassword("admin-password");

    if(!db.open()) {
        qDebug() << "Error connecting to database:" << db.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseConnector::executeQuery(QSqlQuery &query, const QString &queryStr, const QMap<QString, QVariant> &params)
{
    query = QSqlQuery(db);

    query.prepare(queryStr);

    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        qDebug() << "Query exec error: " << query.lastError().text() << " for query: " << queryStr;
        return false;
    }

    return true;
}

UserManager *DatabaseConnector::getUserManager() { return &userManager; }

std::unique_ptr<ChatManager> &DatabaseConnector::getChatManager() { return chatManager; }

std::unique_ptr<GroupManager> &DatabaseConnector::getGroupManager() { return groupManager; }

FileManager *DatabaseConnector::getFileManager() { return &fileManager; }
