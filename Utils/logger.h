#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

class Logger :  public QObject {
    Q_OBJECT
public:
    enum LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    static Logger &instance(QObject *parent = nullptr);
    explicit Logger(const QString &logFilePath = "server.log");
    ~Logger();

    void log(LogLevel level, const QString &module, const QString &message);

private:
    QFile logFile;
    QTextStream logStream;

    QString status;
    QString getLevelString(LogLevel level);
};

#endif // LOGGER_H
