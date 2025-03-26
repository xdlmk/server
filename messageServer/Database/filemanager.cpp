#include "filemanager.h"

#include "databaseconnector.h"

FileManager::FileManager(DatabaseConnector *dbConnector, QObject *parent)
    : QObject{parent} , databaseConnector(dbConnector)
{}

void FileManager::saveFileRecord(const QString &fileUrl)
{
    QMap<QString, QVariant> params;
    params[":fileUrl"] = fileUrl;
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query, "INSERT INTO `files` (`file_url`) VALUES (:fileUrl);", params)) {
        qDebug() << "Error execute sql query:" << query.lastError().text();
    }
}
