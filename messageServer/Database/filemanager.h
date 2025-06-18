#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

#include "../../Utils/logger.h"

class DatabaseConnector;

class FileManager : public QObject
{
    Q_OBJECT
public:
    explicit FileManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    void saveFileRecord(const QString &fileUrl);

    void deleteFile(const quint64& file_id);

private:
    DatabaseConnector *databaseConnector;

    Logger& logger;
};

#endif // FILEMANAGER_H
