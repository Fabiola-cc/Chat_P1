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
#include <QComboBox>
#include <QNetworkReply>
#include "MessageHandler.h"

using namespace std;

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
        messageInput = new QLineEdit( this);
        messageInput->hide();  // Ocultar al inicio
        sendButton = new QPushButton("Enviar", this);
        sendButton->hide();  // Ocultar al inicio
        userList = new QComboBox(this);  // 🔹 Lista de usuarios
        userList->hide();
        errorLabel = new QLabel(this);
        errorLabel->setStyleSheet("color: red; font-weight: bold;");  // Para resaltar errores

        layout->addWidget(hostInput);
        layout->addWidget(portInput);
        layout->addWidget(usernameInput);
        layout->addWidget(connectButton);
        layout->addWidget(statusLabel);
        layout->addWidget(errorLabel); 
        layout->addWidget(chatArea);  // Añadir área de chat
        layout->addWidget(userList);
        layout->addWidget(messageInput);
        layout->addWidget(sendButton);

        setLayout(layout);

        // Conectar señales y slots
        connect(connectButton, &QPushButton::clicked, this, &ChatClient::connectToServer);
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);
        connect(&socket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);

        connect(userList, &QComboBox::currentTextChanged, this, &ChatClient::onUserSelected);  // 🔹 Llamar cuando se selecciona un usuario

        // Crear el manejador de mensajes
        messageHandler = new MessageHandler(socket, messageInput, sendButton, chatArea, userList, this);

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
   

    void onHttpErrorResponse(const QString &message) {
        // Mostrar el mensaje sin caracteres no deseados
        QString cleanedMessage = message;
        cleanedMessage.replace(QRegExp("[\\x00-\\x1F]"), "");  // Elimina caracteres de control
    
        // Muestra el mensaje limpio
        errorLabel->setText("Error del servidor: " + cleanedMessage);
        qDebug() << "Mensaje de error recibido:" << cleanedMessage;
    }
    

    void onDisconnected() {
        statusLabel->setText("Se ha desconectado de la sesión.");
    
        hostInput->show();
        portInput->show();
        usernameInput->show();
        connectButton->show();
        errorLabel->show();
    
        chatArea->hide();
        userList->hide();
        messageInput->hide();
        sendButton->hide();

        connect(&socket, &QWebSocket::textMessageReceived, this, &ChatClient::onHttpErrorResponse);
    }

    void onWebSocketError(QAbstractSocket::SocketError error) {
        Q_UNUSED(error);
        
        // Obtener mensaje de error
        QString errorMsg = socket.errorString();
        
        // Mostrar solo en errorLabel, sin modificar el estado de conexión
        errorLabel->setText("Error: " + errorMsg);
        qDebug() << "Error en WebSocket:" << errorMsg;
    }

    void onConnected() {
        statusLabel->setText("✅ Conectado al servidor WebSocket!");
        reconnectTimer->stop();
    
        // Ocultar los inputs de conexión
        hostInput->hide();
        portInput->hide();
        usernameInput->hide();
        connectButton->hide();
    
        // Mostrar el área de chat y los controles de mensaje
        chatArea->show();
        userList->show();
        messageInput->show();
        sendButton->show();
    
        chatArea->append("🟢 Conectado al chat!");

        // 🔹 Solicitar historial del chat general (~)
        messageHandler->requestChatHistory("~");
    }    


    void attemptReconnect() {
        connectToServer();
    }

    void onUserSelected() {
        QString selectedUser = userList->currentText();
        if (!selectedUser.isEmpty()) {
            messageHandler->requestChatHistory(selectedUser);  // 🔹 Cargar historial del usuario seleccionado
        }
    }

private:
    QWebSocket socket;
    QLabel *statusLabel;
    QLabel *errorLabel;
    QTimer *reconnectTimer;
    QLineEdit *hostInput;
    QLineEdit *portInput;
    QLineEdit *usernameInput;
    QPushButton *connectButton;
    QTextEdit *chatArea;
    QLineEdit *messageInput;
    QPushButton *sendButton;
    QComboBox *userList;
    MessageHandler *messageHandler;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ChatClient client;
    client.show();
    return app.exec();
}
