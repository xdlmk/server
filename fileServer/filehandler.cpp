#include "filehandler.h"

FileHandler::FileHandler(QObject *parent) : QObject{parent}, logger(Logger::instance())
{}

QByteArray FileHandler::getAvatarFromServer(const QByteArray &data)
{
    QDir dir("uploads");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QProtobufSerializer serializer;
    avatars::AvatarRequest request;
    if(!request.deserialize(&serializer,data)){
        logger.log(Logger::INFO,"filehandler.cpp::getAvatarFromServer", "Failed to deserialize protobuf");
        return QByteArray();
    }
    logger.log(Logger::DEBUG,"filehandler.cpp::getAvatarFromServer", "Method starts with url: " + request.avatarUrl());

    QFile file("uploads/" + request.avatarUrl());

    QByteArray fileData;
    if(file.open(QIODevice::ReadOnly)) {
        fileData = file.readAll();
        file.close();
    }

    avatars::AvatarData response;
    response.setType(request.type());
    response.setUserId(request.userId());
    response.setAvatarUrl(request.avatarUrl());
    response.setAvatarData(fileData);

    return response.serialize(&serializer);
}

QByteArray FileHandler::makeAvatarUrlProcessing(const QByteArray &data)
{
    QProtobufSerializer serializer;
    avatars::AvatarFileData inMsg;
    if (!inMsg.deserialize(&serializer, data)) {
        logger.log(Logger::WARN, "filehandler.cpp::makeAvatarUrlProcessing", "Failed to deserialize AvatarFileData");
        return QByteArray();
    }

    QString fileName = inMsg.fileName();
    QString fileExtension = inMsg.fileExtension();
    QString avatarUrl = makeUrlProcessing(fileName, fileExtension, inMsg.fileData());

    quint64 id = inMsg.id_proto();
    QString msgType = inMsg.type();

    if(msgType == "personal"){
        emit setAvatarInDatabase(avatarUrl,id);
    } else if(msgType == "group"){
        emit setGroupAvatarInDatabase(avatarUrl,id);
    }

    avatars::AvatarRequest response;
    response.setAvatarUrl(avatarUrl);
    response.setUserId(id);
    response.setType(msgType);

    /*QJsonObject avatarUrlJson;
    avatarUrlJson["flag"] = "avatarUrl";
    avatarUrlJson["type"] = json["type"].toString();
    avatarUrlJson["avatar_url"] = avatarUrl;
    avatarUrlJson["id"] = id;*/

    return response.serialize(&serializer);
}

QString FileHandler::makeUrlProcessing(const QString &fileName, const QString &fileExtension, const QByteArray &data)
{
    logger.log(Logger::DEBUG,"filehandler.cpp::makeUrlProcessing", "Method starts");

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
    file.write(data);
    file.close();

    logger.log(Logger::DEBUG,"filehandler.cpp::makeUrlProcessing", "File saved as: " + uniqueFileName);
    emit saveFileToDatabase(uniqueFileName);

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
    QString fileUrl = makeUrlProcessing(createGroupJson["fileName"].toString(),
                                        createGroupJson["fileExtension"].toString(),
                                        QByteArray::fromBase64(createGroupJson["fileData"].toString().toUtf8())
                                        );
    createGroupJson.remove("fileData");
    createGroupJson["avatar_url"] = fileUrl;
    createGroupJson.remove("fileName");
    createGroupJson.remove("fileExtension");

    emit createGroup(createGroupJson);
}
