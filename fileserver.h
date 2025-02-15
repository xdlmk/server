#ifndef FILESERVER_H
#define FILESERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <QFile>
#include <QDir>
#include <QByteArray>

#include <QJsonObject>
#include <QJsonDocument>

#include <QDateTime>

#include <QDebug>
#include <QUuid>

class FileServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit FileServer(QObject *parent = nullptr);

signals:
    void saveFileToDatabase(const QString& fileUrl);
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;
private slots:
    void sendData(QJsonDocument& sendDoc);
    void readClient();
    void makeUrlProcessing(const QJsonObject& json);
    void getFileFromUrlProcessing(const QString& fileUrl);
    void getAvatarFromServer(const QJsonObject &json);

private:
    QTcpSocket* socket;
};

#endif // FILESERVER_H
