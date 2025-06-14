#include <QCoreApplication>
#include <QLocalSocket>
#include <QStringList>
#include <iostream>

#define SERVER_SOCKET_NAME "mserver_socket"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();

    if (args.size() < 2) {
        std::cout << "Usage:\n";
        std::cout << "  mmserver_cli --stop\n";
        std::cout << "  mmserver_cli --delete-user <id>\n";
        return 1;
    }

    QString command;
    if (args[1] == "--stop") {
        command = "stop";
    } else if (args[1] == "--delete-user" && args.size() >= 3) {
        command = "delete-user " + args[2];
    } else {
        std::cerr << "Unknown command\n";
        return 1;
    }

    QLocalSocket socket;
    socket.connectToServer(SERVER_SOCKET_NAME);
    if (!socket.waitForConnected(1000)) {
        std::cerr << "Failed to connect to server socket: "
                  << socket.errorString().toStdString() << "\n";
        return 1;
    }

    socket.write(command.toUtf8());
    socket.flush();

    if (!socket.waitForReadyRead(1000)) {
        std::cerr << "No response from server.\n";
        return 1;
    }

    QByteArray response = socket.readAll();
    std::cout << "Server response: " << response.trimmed().toStdString() << "\n";

    socket.disconnectFromServer();
    return 0;
}
