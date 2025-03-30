#include "databaseconnector.h"

#include "groupmanager.h"
#include "chatmanager.h"
#include "usermanager.h"

DatabaseConnector &DatabaseConnector::instance(QObject *parent) {
    static DatabaseConnector instance(parent);
    return instance;
}

DatabaseConnector::DatabaseConnector(QObject *parent)
    : QObject{parent}, fileManager(this), logger(Logger::instance())
{
    groupManager = std::make_unique<GroupManager>(this);
    chatManager = std::make_unique<ChatManager>(this);
    userManager = std::make_unique<UserManager>(this);
    while(!connectToDatabase()) {
        logger.log(Logger::WARN,"databaseconnector.cpp::constructor", "Connect to database failed with error: " + db.lastError().text());
    }
    logger.log(Logger::DEBUG,"databaseconnector.cpp::constructor","Database connected");
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
        logger.log(Logger::WARN,"databaseconnector.cpp::executeQuery", "Query exec error: " + query.lastError().text() + " for query: " + queryStr);
        return false;
    }

    return true;
}

std::unique_ptr<UserManager> &DatabaseConnector::getUserManager() { return userManager; }

std::unique_ptr<ChatManager> &DatabaseConnector::getChatManager() { return chatManager; }

std::unique_ptr<GroupManager> &DatabaseConnector::getGroupManager() { return groupManager; }

FileManager *DatabaseConnector::getFileManager() { return &fileManager; }
