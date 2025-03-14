#include <QApplication>
#include <QWebSocket>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <iostream>

class WebSocketClient : public QWidget {
    Q_OBJECT

public:
    WebSocketClient(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("Cliente WebSocket");
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        statusLabel = new QLabel("Esperando conexión...", this);
        layout->addWidget(statusLabel);
        setLayout(layout);

        // Conectar señal de conexión del WebSocket a un slot de esta clase
        connect(&socket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
        
        // Iniciar la conexión
        socket.open(QUrl("ws://localhost:8080"));
    }

public slots:
    void onConnected() {
        std::cout << "✅ Conectado al servidor WebSocket!" << std::endl;
        statusLabel->setText("✅ Conectado al servidor WebSocket!");
    }

private:
    QWebSocket socket;
    QLabel *statusLabel;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    WebSocketClient client;
    client.show();

    return app.exec();  // Mantiene el bucle de eventos activo
}

