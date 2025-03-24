#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

class DatabaseConnector;

class FileManager : public QObject
{
    Q_OBJECT
public:
    explicit FileManager(DatabaseConnector *dbConnector, QObject *parent = nullptr);

    void saveFileRecord(const QString &fileUrl);  // DatabaseManager::saveFileToDatabase

private:
    DatabaseConnector *databaseConnector;
};

#endif // FILEMANAGER_H
