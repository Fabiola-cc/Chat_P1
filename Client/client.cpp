#include <QApplication>
#include <QWebSocket>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <iostream>

class ChatClient : public QWidget {
    Q_OBJECT

public:
    ChatClient(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("ChatCliente WebSocket");

        QVBoxLayout *layout = new QVBoxLayout(this);
        statusLabel = new QLabel("Esperando conexión...", this);
        layout->addWidget(statusLabel);
        setLayout(layout);

        // Conectar señales de WebSocket a slots de esta clase
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);
        connect(&socket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);

        // Inicializar temporizador de reconexión, pero no arrancarlo aún
        reconnectTimer = new QTimer(this);
        reconnectTimer->setInterval(5000);  // Intentar reconectar cada 5 segundos
        connect(reconnectTimer, &QTimer::timeout, this, &ChatClient::attemptReconnect);

        // Intentar la conexión inicial
        connectToServer();
    }

public slots:
    void onConnected() {
        statusLabel->setText("✅ Conectado al servidor WebSocket!");
        reconnectTimer->stop();
    }

    void onDisconnected() {
        statusLabel->setText("❌ Desconectado. Intentando reconectar...");
        reconnectTimer->start();
    }

    void attemptReconnect() {
        connectToServer();
    }

private:
    void connectToServer() {
        socket.open(QUrl("ws://localhost:8080"));
    }

private:
    QWebSocket socket;
    QLabel *statusLabel;
    QTimer *reconnectTimer;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    ChatClient client;
    client.show();

    return app.exec();  // Mantiene el bucle de eventos activo
}
