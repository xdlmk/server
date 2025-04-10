#include "filehandler.h"

FileHandler::FileHandler(QObject *parent) : QObject{parent}, logger(Logger::instance())
{}

QJsonObject FileHandler::getAvatarFromServer(const QJsonObject &json)
{
    logger.log(Logger::DEBUG,"filehandler.cpp::getAvatarFromServer", "Method starts with url: " + json["avatar_url"].toString());
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
    int id = json["id"].toInt();
    if(json["type"].toString() == "personal"){
        emit setAvatarInDatabase(avatarUrl,id);
    } else if(json["type"].toString() == "group"){
        emit setGroupAvatarInDatabase(avatarUrl,id);
    }

    QJsonObject avatarUrlJson;
    avatarUrlJson["flag"] = "avatarUrl";
    avatarUrlJson["type"] = json["type"].toString();
    avatarUrlJson["avatar_url"] = avatarUrl;
    avatarUrlJson["id"] = id;

    return avatarUrlJson;
}

QString FileHandler::makeUrlProcessing(const QJsonObject &json)
{
    logger.log(Logger::DEBUG,"filehandler.cpp::makeUrlProcessing", "Method starts");
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
        logger.log(Logger::WARN,"filehandler.cpp::makeUrlProcessing", "Failed to save file: " + uniqueFileName + " with error: " + file.errorString());
        return "";
    }
    file.write(fileData);
    file.close();

    logger.log(Logger::DEBUG,"filehandler.cpp::makeUrlProcessing", "File saved as: " + uniqueFileName);
    emit saveFileToDatabase(uniqueFileName);

    QJsonObject fileUrl;
    return uniqueFileName;
}

QJsonObject FileHandler::getFileFromUrlProcessing(const QString &fileUrl, const QString &flag)
{
    logger.log(Logger::DEBUG,"filehandler.cpp::getFileFromUrlProcessing", "Method starts");
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
    logger.log(Logger::DEBUG,"filehandler.cpp::voiceMessageProcessing", "Method starts");
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
        logger.log(Logger::WARN,"filehandler.cpp::voiceMessageProcessing", "Failed to save file: " + uniqueFileName + " with error: " + file.errorString());
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

void FileHandler::fileMessageProcessing(QJsonObject &fileJson)
{
    logger.log(Logger::DEBUG,"filehandler.cpp::fileMessageProcessing", "Method starts");
    QString fileName = fileJson["fileName"].toString();
    QString fileExtension = fileJson["fileExtension"].toString();
    QString fileDataBase64 = fileJson["fileData"].toString();
    QByteArray fileData = QByteArray::fromBase64(fileDataBase64.toUtf8());
    QString uniqueFileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + fileName + "." + fileExtension;

    QDir dir("uploads");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QFile file("uploads/" + uniqueFileName);
    if (!file.open(QIODevice::WriteOnly)) {
        logger.log(Logger::WARN,"filehandler.cpp::fileMessageProcessing", "Failed to save file: " + uniqueFileName + " with error: " + file.errorString());
    }
    file.write(fileData);
    file.close();
    fileJson.remove("fileData");
    fileJson.remove("fileName");
    fileJson.remove("fileExtension");
    fileJson["fileUrl"] = uniqueFileName;

    emit saveFileToDatabase(uniqueFileName);
}

void FileHandler::createGroupWithAvatarProcessing(QJsonObject &createGroupJson)
{
    QString fileUrl = makeUrlProcessing(createGroupJson);
    createGroupJson.remove("fileData");
    createGroupJson["avatar_url"] = fileUrl;
    createGroupJson.remove("fileName");
    createGroupJson.remove("fileExtension");

    emit createGroup(createGroupJson);
}
