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
        logger.log(Logger::INFO,"filemanager.cpp::saveFileRecord", "Query exec error: " + query.lastError().text());
    }
}

void FileManager::deleteFile(const quint64 &file_id)
{
    QMap<QString, QVariant> params;
    params[":id_file"] = QVariant::fromValue(file_id);
    QSqlQuery query;

    if (!databaseConnector->executeQuery(query,
                                         "SELECT COUNT(*) FROM files WHERE file_id = :id_file;",
                                         params)) {

        logger.log(Logger::DEBUG, "filemanager.cpp::deleteFile",
                   "Query failed: " + query.lastError().text());
        throw std::runtime_error("Query failed: " + query.lastError().text().toStdString());
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        if(count <= 0) throw std::runtime_error("File not found");
    } else throw std::runtime_error("File not found");

    if (!databaseConnector->executeQuery(query,
                                         "DELETE FROM files WHERE file_id = :id_file;",
                                         params)) {

        logger.log(Logger::DEBUG, "filemanager.cpp::deleteFile",
                   "Delete failed: " + query.lastError().text());
        throw std::runtime_error("Failed to delete file from database");
    }
}
