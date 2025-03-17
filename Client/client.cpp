#include <QApplication>
#include <QWebSocket>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <iostream>

class ChatClient : public QWidget {
    Q_OBJECT

public:
    ChatClient(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("Chat");

        QVBoxLayout *layout = new QVBoxLayout(this);

        // Entrada de host, puerto y usuario
        hostInput = new QLineEdit("localhost", this);
        portInput = new QLineEdit("8080", this);
        usernameInput = new QLineEdit("Usuario", this);
        connectButton = new QPushButton("Conectar", this);

        statusLabel = new QLabel("🔹 Introduce los datos y presiona conectar.", this);

        // Área de chat (inicialmente oculta)
        chatArea = new QTextEdit(this);
        chatArea->setReadOnly(true);
        chatArea->hide();  // Ocultar al inicio

        layout->addWidget(hostInput);
        layout->addWidget(portInput);
        layout->addWidget(usernameInput);
        layout->addWidget(connectButton);
        layout->addWidget(statusLabel);
        layout->addWidget(chatArea);  // Añadir área de chat

        setLayout(layout);

        // Conectar señales y slots
        connect(connectButton, &QPushButton::clicked, this, &ChatClient::connectToServer);
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);
        connect(&socket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);
        connect(&socket, &QWebSocket::textMessageReceived, this, &ChatClient::onMessageReceived);

        // Temporizador de reconexión
        reconnectTimer = new QTimer(this);
        reconnectTimer->setInterval(5000);  // Intentar reconectar cada 5 segundos
        connect(reconnectTimer, &QTimer::timeout, this, &ChatClient::attemptReconnect);
    }

public slots:
    void connectToServer() {
        QString host = hostInput->text();
        QString port = portInput->text();
        QString username = usernameInput->text();

        if (host.isEmpty() || port.isEmpty() || username.isEmpty()) {
            statusLabel->setText("⚠️ Todos los campos son obligatorios.");
            return;
        }

        QString url = QString("ws://%1:%2?name=%3").arg(host, port, username);
        statusLabel->setText("🔄 Conectando a " + url + "...");
        socket.open(QUrl(url));
    }

    void onConnected() {
        statusLabel->setText("✅ Conectado al servidor WebSocket!");
        reconnectTimer->stop();

        // Ocultar los inputs y el botón
        hostInput->hide();
        portInput->hide();
        usernameInput->hide();
        connectButton->hide();

        // Mostrar la interfaz de chat
        chatArea->show();
        chatArea->append("🟢 Conectado al chat!");
    }

    void onDisconnected() {
        statusLabel->setText("❌ Desconectado.");
    }

    void attemptReconnect() {
        connectToServer();
    }

    void onMessageReceived(const QString &message) {
        chatArea->append("💬 " + message);
    }

private:
    QWebSocket socket;
    QLabel *statusLabel;
    QTimer *reconnectTimer;
    QLineEdit *hostInput;
    QLineEdit *portInput;
    QLineEdit *usernameInput;
    QPushButton *connectButton;
    QTextEdit *chatArea;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ChatClient client;
    client.show();
    return app.exec();
}
