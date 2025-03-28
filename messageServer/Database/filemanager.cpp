#include "filemanager.h"

#include "databaseconnector.h"

FileManager::FileManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector), logger(Logger::instance())
{}

void FileManager::saveFileRecord(const QString &fileUrl)
{
    QMap<QString, QVariant> params;
    params[":fileUrl"] = fileUrl;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query, "INSERT INTO `files` (`file_url`) VALUES (:fileUrl);", params)) {
        logger.log(Logger::WARN,"filemanager.cpp::saveFileRecord", "Query exec error: " + query.lastError().text());
    }
}
