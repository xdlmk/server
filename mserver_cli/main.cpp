#include <QCoreApplication>
#include <QLocalSocket>
#include <QStringList>
#include <QCommandLineParser>
#include <QTextStream>
#include <iostream>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define SERVER_SOCKET_NAME "mserver_socket"

void printResponse(const QString& response, bool error = false) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD saved_attributes = 0;

    if (error && GetConsoleScreenBufferInfo(hConsole, &consoleInfo)) {
        saved_attributes = consoleInfo.wAttributes;
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }

    QTextStream stream(error ? stderr : stdout);
    stream << response << "\n";

    if (error) {
        SetConsoleTextAttribute(hConsole, saved_attributes);
    }

#else
    QTextStream stream(error ? stderr : stdout);
    if (error) {
        stream << "\033[1;31m";
    }

    stream << response << "\n";

    if (error) {
        stream << "\033[0m";
    }
#endif
}

QString buildCommand(const QCommandLineParser& parser) {
    if (parser.isSet("stop")) return "stop";
    if (parser.isSet("status")) return "status";
    if (parser.isSet("delete-user")) return "delete-user " + parser.value("delete-user");
    if (parser.isSet("delete-group")) return "delete-group " + parser.value("delete-group");
    if (parser.isSet("delete-message")) return "delete-message " + parser.value("delete-message");
    if (parser.isSet("delete-file")) return "delete-file " + parser.value("delete-file");
    if (parser.isSet("group-members")) {
        QString cmd = "group-members " + parser.value("group-members");
        if (parser.isSet("full-info")) {
            cmd += " --full-info";
        }
        return cmd;
    }
    if (parser.isSet("user-info")) return "user-info " + parser.value("user-info");

    return "";
}

bool sendCommand(const QString& command, QString& response) {
    QLocalSocket socket;
    socket.connectToServer(SERVER_SOCKET_NAME);
    if (!socket.waitForConnected(1000)) {
        response = "Failed to connect: " + socket.errorString();
        return false;
    }

    socket.write(command.toUtf8());
    socket.flush();

    if (!socket.waitForReadyRead(2000)) {
        response = "No response from server.";
        return false;
    }

    response = QString::fromUtf8(socket.readAll()).trimmed();
    socket.disconnectFromServer();
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("CLI for managing mserver instance");

    parser.addOption({"stop", "Stop the server"});
    parser.addOption({"status", "Check server status"});
    parser.addOption({"delete-user", "Delete user by ID", "id"});
    parser.addOption({"delete-group", "Delete group by ID", "group_id"});
    parser.addOption({"delete-message", "Delete message by ID", "msg_id"});
    parser.addOption({"delete-file", "Delete file by ID", "file_id"});
    parser.addOption({"group-members", "List members of a group", "group_id"});
    parser.addOption({"full-info", "Show full information about users in group"});
    parser.addOption({"user-info", "Show information about user by ID", "user_id"});
    parser.addOption({{"help","h","?"}, "Show list of available commands"});

    const bool parsed = parser.parse(QCoreApplication::arguments());
    if (!parsed) {
        printResponse(parser.errorText() + "\nEnter '--help' for a list of available commands.", true);
        return 1;
    }

    if (parser.isSet("help") || parser.isSet("h") || parser.isSet("?")) {
        QTextStream out(stdout);
        out << "Usage: " << QCoreApplication::applicationFilePath() << " [options]\n";
        out << parser.applicationDescription() << "\n\n";
        out << "Options:\n";
        out << "  --stop                      Stop the server\n";
        out << "  --status                    Check server status\n";
        out << "  --delete-user <id>          Delete user by ID\n";
        out << "  --delete-group <group_id>   Delete group by ID\n";
        out << "  --delete-message <msg_id>   Delete message by ID\n";
        out << "  --delete-file <file_id>     Delete file by ID\n";
        out << "\n  --group-members <group_id>  List members of a group\n";
        out << "  --full-info                 Show detailed info for group members (used with --group-members)\n\n";
        out << "  --user-info <user_id>       Show info about a specific user\n";
        out << "  -h, -?, --help              Show this help text\n";
        return 0;
    }

    QString command = buildCommand(parser);
    if (command.isEmpty()) {
        printResponse("No valid command provided. Use --help for usage.", true);
        return 2;
    }

    QString response;
    if (!sendCommand(command, response)) {
        printResponse("Error: " + response, true);
        return 1;
    }

    printResponse("Server response:\n" + response);
    return 0;
}
