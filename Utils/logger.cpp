#include "Logger.h"
#include <QDebug>

Logger &Logger::instance(QObject *parent) {
    static Logger instance;
    return instance;
}

Logger::Logger(const QString &logFilePath) : logFile(logFilePath),status("standart") {
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed toopen log file:" << logFilePath;
    }
    logStream.setDevice(&logFile);
}

Logger::~Logger() {
    if (logFile.isOpen()) {
        logFile.close();
    }
}

void Logger::log(LogLevel level, const QString &module, const QString &message) {
    if(status == "errorsonly" && (level == Logger::INFO || level == Logger::DEBUG)) return;
    if(status == "noinfo" && level == Logger::INFO) return;
    if(status == "fatalonly" && (level == Logger::INFO || level == Logger::DEBUG || level == Logger::WARN || level == Logger::ERROR)) return;
    QString levelString = getLevelString(level);
    QString logMessage = QString("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                             .arg(levelString)
                             .arg(module)
                             .arg(message);

    logStream << logMessage << "\n";
    logStream.flush();

    qDebug() << logMessage;
    emit newLogMessage(logMessage);
}

bool Logger::setLogStatus(const QString &status)
{
    this->status = status;
    return true;
}

QString Logger::getLevelString(LogLevel level) {
    switch (level) {
    case DEBUG: return "DEBUG";
    case INFO: return "INFO";
    case WARN: return "WARN";
    case ERROR: return "ERROR";
    case FATAL: return "FATAL";
    default: return "UNKNOWN";
    }
}
