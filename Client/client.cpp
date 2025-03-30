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
#include "OptionsDialog.h"

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
        optionsButton = new QPushButton("Opciones", this);


        statusLabel = new QLabel("Introduce los datos y presiona conectar.", this);
        errorLabel = new QLabel(this);
        errorLabel->setStyleSheet("color: red; font-weight: bold;");  // Para resaltar errores

        // Área de chat (inicialmente oculta)
        chatArea = new QTextEdit(this);
        chatArea->setReadOnly(true);
        chatArea->hide();  // Ocultar al inicio
        messageInput = new QLineEdit( this);
        messageInput->hide();  // Ocultar al inicio
        sendButton = new QPushButton("Enviar", this);
        sendButton->hide();  // Ocultar al inicio
        userList = new QComboBox(this);  // 🔹 Lista de usuarios
        userList->addItem("General");
        userList->hide();
        optionsButton->hide();
       
        statusDropdown = new QComboBox(this);
        statusDropdown->addItem("Activo", 1);
        statusDropdown->addItem("Ocupado", 2);
        statusDropdown->addItem("Inactivo", 3);
        statusDropdown->setEnabled(false);
        statusDropdown->hide();

        layout->addWidget(hostInput);
        layout->addWidget(portInput);
        layout->addWidget(usernameInput);
        layout->addWidget(connectButton);
        layout->addWidget(statusLabel);
        layout->addWidget(errorLabel); 
        layout->addWidget(statusDropdown);
        layout->addWidget(chatArea);  // Añadir área de chat
        layout->addWidget(userList);
        layout->addWidget(messageInput);
        layout->addWidget(sendButton);
        layout->addWidget(optionsButton);

        setLayout(layout);

        // Conectar señales y slots
        connect(connectButton, &QPushButton::clicked, this, &ChatClient::connectToServer);
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);
        connect(&socket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);
        connect(optionsButton, &QPushButton::clicked, this, &ChatClient::showOptionsDialog);


        connect(userList, &QComboBox::currentTextChanged, this, &ChatClient::onUserSelected);  // 🔹 Llamar cuando se selecciona un usuario

        // Crear el manejador de mensajes
        messageHandler = new MessageHandler(socket, messageInput, sendButton, chatArea, userList, statusDropdown, usernameInput, this);

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
            statusLabel->setText(" Todos los campos son obligatorios.");
            return;
        }

        // Crear la URL del WebSocket
        QString url = QString("ws://%1:%2?name=%3").arg(host, port, username);
        statusLabel->setText("🔄 Conectando a " + url + "...");

        // Imprimir la URL para asegurarnos de que está bien formada
        qDebug() << "Conectando a la URL: " << url;

        // Conectar la señal de conexión exitosa
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);
        // Conectar la señal de error
        connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &ChatClient::onSocketError);

        // Intentar abrir la conexión WebSocket
        socket.open(QUrl(url));
    }


    void onSocketError(QAbstractSocket::SocketError error) {
        QString errorMessage;
        switch (error) {
            case QAbstractSocket::HostNotFoundError:
                errorMessage = "Host no encontrado. Verifica la dirección del servidor.";
                break;
            case QAbstractSocket::ConnectionRefusedError:
                errorMessage = "Conexión rechazada por el servidor. Puede que el servidor no esté disponible.";
                break;
            case QAbstractSocket::RemoteHostClosedError:
                errorMessage = "El host remoto cerró la conexión.";
                break;
            case QAbstractSocket::SocketAccessError:
                errorMessage = "Acceso denegado al socket.";
                break;
            case QAbstractSocket::SocketResourceError:
                errorMessage = "Recursos insuficientes para establecer la conexión.";
                break;
            case QAbstractSocket::SocketTimeoutError:
                errorMessage = "Tiempo de espera agotado.";
                break;
            default:
                errorMessage = "Error: " + socket.errorString();
                break;
        }
        
        qDebug() << "Error de conexión WebSocket: " << errorMessage;
        // Mostrar mensaje de error
        statusLabel->setText("❌ Error: " + errorMessage);
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
        optionsButton->hide();


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
        statusLabel->setText("Conectado al Chat");
        reconnectTimer->stop();
        statusDropdown->setEnabled(true);
    
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
        optionsButton->show();
        statusDropdown->show();

    
        chatArea->append("Conectado al chat!");

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

    void showOptionsDialog() {
        OptionsDialog *dialog = new OptionsDialog(this);
    
        // Pasar la lista de usuarios al diálogo
        dialog->setUserList(userList);
        
        // Configuramos el diálogo para que se auto-destruya cuando se cierre
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        
        // Mostramos el diálogo de forma no-modal
        dialog->show();
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
    QPushButton *optionsButton;
    QTextEdit *chatArea;
    QLineEdit *messageInput;
    QPushButton *sendButton;
    QComboBox *userList;
    QComboBox *statusDropdown;
    MessageHandler *messageHandler;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ChatClient client;
    client.show();
    return app.exec();
}
