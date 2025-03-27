#include "Logger.h"
#include <QDebug>

Logger &Logger::instance(QObject *parent) {
    static Logger instance(parent);
    return instance;
}

Logger::Logger(const QString &logFilePath) : logFile(logFilePath) {
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
    QString levelString = getLevelString(level);
    QString logMessage = QString("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                             .arg(levelString)
                             .arg(module)
                             .arg(message);

    logStream << logMessage << "\n";
    logStream.flush();

    qDebug() << logMessage;
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
