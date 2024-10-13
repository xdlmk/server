#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>

#include <QList>
#include <QMultiMap>

#include <QJsonObject>
#include <QJsonDocument>

#include <QImage>
#include <QBuffer>
#include <QImageReader>

#include <QSqlDatabase>
#include <QtSql>
#include <QSqlQuery>

#include <QCryptographicHash>

class ChatServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);

signals:

    // QTcpServer interface
protected:
    void incomingConnection(qintptr handle) Q_DECL_OVERRIDE;

private slots:
    void readClient();
    void disconnectClient();
    void SendToClient(QJsonDocument doc,const QString& senderLogin);

private:
    QString convertImageToBase64(const QString &filePath);

    QString hashPassword(const QString &password);
    bool checkPassword(const QString &enteredPassword, const QString &storedHash);

    void sendJson(const QJsonDocument &sendDoc);
    QJsonObject loginProcess(QJsonObject json);
    QJsonObject regProcess(QJsonObject json);
    void connectToDB();

    QTcpSocket *socket;
    QByteArray data;
    QMultiMap <QString, QTcpSocket*> clients;
};

#endif // CHATSERVER_H
