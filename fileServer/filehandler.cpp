#include "filehandler.h"

FileHandler::FileHandler(QObject *parent) : QObject{parent} {

}

QJsonObject FileHandler::getAvatarFromServer(const QJsonObject &json)
{
    qDebug() << "getAvatarFromServer starts with url " + json["avatar_url"].toString();
    QDir dir("uploads");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file("uploads/" + json["avatar_url"].toString());

    QByteArray fileData;
    if(file.open(QIODevice::ReadOnly)) {
        fileData = file.readAll();
        file.close();
    }
    QJsonObject fileDataJson;
    fileDataJson["flag"] = "avatarData";
    fileDataJson["type"] = json["type"].toString();
    fileDataJson["user_id"] = json["user_id"].toInt();
    fileDataJson["avatar_url"] = json["avatar_url"].toString();
    fileDataJson["avatarData"] = QString(fileData.toBase64());

    return fileDataJson;
}

QJsonObject FileHandler::makeAvatarUrlProcessing(const QJsonObject &json)
{
    QString avatarUrl = makeUrlProcessing(json);
    int user_id = json["user_id"].toInt();
    emit setAvatarInDatabase(avatarUrl,user_id);

    QJsonObject avatarUrlJson;
    avatarUrlJson["flag"] = "avatarUrl";
    avatarUrlJson["type"] = json["type"];
    avatarUrlJson["avatar_url"] = avatarUrl;
    avatarUrlJson["user_id"] = user_id;

    return avatarUrlJson;
}

QString FileHandler::makeUrlProcessing(const QJsonObject &json)
{
    qDebug() << "makeUrlProcessing starts" ;
    QString fileName = json["fileName"].toString();
    QString fileExtension = json["fileExtension"].toString();
    QString fileDataBase64 = json["fileData"].toString();
    QByteArray fileData = QByteArray::fromBase64(fileDataBase64.toUtf8());

    QString uniqueFileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + fileName + "." + fileExtension;

    QDir dir("uploads");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QFile file("uploads/" + uniqueFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save file:" << uniqueFileName;
        return "";
    }
    file.write(fileData);
    file.close();

    qDebug() << "File saved as:" << uniqueFileName;
    emit saveFileToDatabase(uniqueFileName);

    QJsonObject fileUrl;
    return uniqueFileName;
}

QJsonObject FileHandler::getFileFromUrlProcessing(const QString &fileUrl, const QString &flag)
{
    qDebug() << "getFileFromUrlProcessing starts";
    QString folder;
    if(flag == "fileData") folder = "uploads";
    else if(flag == "voiceFileData") folder = "voice_messages";

    QFile file(folder + "/" + fileUrl);
    QByteArray fileData;
    if(file.open(QIODevice::ReadOnly)) {
        fileData = file.readAll();
        file.close();
    }
    QJsonObject fileDataJson;
    fileDataJson["flag"] = flag;
    fileDataJson["fileName"] = fileUrl;
    fileDataJson["fileData"] = QString(fileData.toBase64());

    return fileDataJson;
}

void FileHandler::voiceMessageProcessing(QJsonObject &voiceJson)
{
    qDebug() << "voiceMessageProcessing starts";
    QString fileName = voiceJson["fileName"].toString();
    QString fileExtension = voiceJson["fileExtension"].toString();
    QString fileDataBase64 = voiceJson["fileData"].toString();
    QByteArray fileData = QByteArray::fromBase64(fileDataBase64.toUtf8());
    QString uniqueFileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + fileName + "." + fileExtension;

    QDir dir("voice_messages");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QFile file("voice_messages/" + uniqueFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save file:" << uniqueFileName;
    }
    file.write(fileData);
    file.close();
    voiceJson.remove("fileData");
    voiceJson.remove("fileName");
    voiceJson.remove("fileExtension");
    voiceJson["fileUrl"] = uniqueFileName;
    voiceJson["message"] = "";

    emit saveFileToDatabase(uniqueFileName);
}

void FileHandler::createGroupWithAvatarProcessing(QJsonObject &createGroupJson)
{
    QString fileUrl = makeUrlProcessing(createGroupJson);
    createGroupJson.remove("fileData");
    createGroupJson["avatar_url"] = fileUrl;
    qDebug() << "createGroupWithAvatarProcessing json " << createGroupJson;
    createGroupJson.remove("fileName");
    createGroupJson.remove("fileExtension");

    emit createGroup(createGroupJson);
}
