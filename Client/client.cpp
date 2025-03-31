#include <QApplication>
#include <QWebSocket>      // Para la comunicación WebSocket
#include <QLabel>          // Para etiquetas de texto
#include <QVBoxLayout>     // Para organizar widgets verticalmente
#include <QWidget>         // Clase base para todos los widgets de la UI
#include <QTimer>          // Para temporizadores
#include <QLineEdit>       // Para campos de entrada de texto
#include <QPushButton>     // Para botones
#include <QTextEdit>       // Para áreas de texto multilinea
#include <iostream>
#include <QComboBox>       // Para menús desplegables
#include <QNetworkReply>   // Para manejar respuestas de red
#include "MessageHandler.h" // Clase personalizada para manejo de mensajes
#include "OptionsDialog.h"  // Diálogo de opciones personalizado

using namespace std;

/**
 * @class ChatClient
 * @brief Clase principal del cliente de chat
 * 
 * Implementa una interfaz de chat basada en WebSockets que permite
 * conectarse a un servidor, enviar mensajes y gestionar la lista de usuarios.
 */
class ChatClient : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor del cliente de chat
     * @param parent Widget padre (opcional)
     * 
     * Inicializa la interfaz gráfica y configura las conexiones entre señales y slots.
     */
    ChatClient(QWidget *parent = nullptr) : QWidget(parent) {
        resize(800, 400); // Establecer tamaño inicial
        setWindowTitle("Chat");

        // Layout principal horizontal para colocar los dos paneles lado a lado
        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // Controles comunes para la configuración de conexión (en la parte superior)
        QWidget *configPanel = new QWidget(this);
        QHBoxLayout *configLayout = new QHBoxLayout(configPanel);
        
        // Panel izquierdo
        QWidget *leftPanel = new QWidget(this);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
        
        // Panel derecho
        QWidget *rightPanel = new QWidget(this);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

        // Controles para la configuración de conexión
        hostInput = new QLineEdit("localhost", this);        // Campo para el host del servidor
        portInput = new QLineEdit("8080", this);             // Campo para el puerto
        usernameInput = new QLineEdit("Usuario", this);      // Campo para el nombre de usuario
        connectButton = new QPushButton("Conectar", this);   // Botón para iniciar conexión
        optionsButton = new QPushButton("Opciones", this);   // Botón para opciones adicionales

        // Etiquetas para mostrar estado y errores
        statusLabel = new QLabel("Introduce los datos y presiona conectar.", this);
        errorLabel = new QLabel(this);
        errorLabel->setStyleSheet("color: red; font-weight: bold;");  // Estilo para errores

        // Añadir todos los controles al layout vertical
        mainLayout->addWidget(hostInput);
        mainLayout->addWidget(portInput);
        mainLayout->addWidget(usernameInput);
        mainLayout->addWidget(connectButton);
        mainLayout->addWidget(statusLabel);
        mainLayout->addWidget(errorLabel); 

        // Componentes de la interfaz de chat (inicialmente ocultos)
        // Menú desplegable para estado del usuario
        statusDropdown = new QComboBox(this);
        statusDropdown->addItem("Activo", 1);       // Estado activo (valor 1)
        statusDropdown->addItem("Ocupado", 2);      // Estado ocupado (valor 2)
        statusDropdown->addItem("Inactivo", 3);     // Estado inactivo (valor 3)
        statusDropdown->setEnabled(false);          // Deshabilitado hasta que se conecte
        statusDropdown->hide();
        notificationLabel = new QLabel(this);
        notificationLabel->setStyleSheet("color: blue; font-weight: bold;"); // Estilo azul
        notificationLabel->hide();                     // Oculto hasta que se conecte

        notificationTimer = new QTimer(this);
        notificationTimer->setSingleShot(true); // Solo se ejecuta una vez por evento
        connect(notificationTimer, &QTimer::timeout, this, [this]() {
            notificationLabel->clear();
            notificationLabel->hide();
        });

        mainLayout->addWidget(statusDropdown);
        mainLayout->addWidget(notificationLabel);

        // Chat IZQUIERDO
        generalChatLabel = new QLabel("Chat general: ", this);
        generalChatLabel->hide();
        generalChatArea = new QTextEdit(this);             // Área donde se muestran los mensajes
        generalChatArea->setReadOnly(true);                // Solo lectura para evitar edición
        generalChatArea->hide();                           // Oculto hasta que se conecte
        generalMessageInput = new QLineEdit(this);         // Campo para escribir mensajes
        generalMessageInput->hide();                       // Oculto hasta que se conecte
        generalSendButton = new QPushButton("Enviar", this); // Botón para enviar mensajes
        generalSendButton->hide();                         // Oculto hasta que se conecte

        leftLayout->addWidget(generalChatLabel);
        leftLayout->addWidget(generalChatArea);
        leftLayout->addWidget(generalMessageInput);
        leftLayout->addWidget(generalSendButton);

        // Chat DERECHO
        chatLabel = new QLabel("Chat personal: ", this);
        chatLabel->hide();
        chatArea = new QTextEdit(this);             // Área donde se muestran los mensajes
        chatArea->setReadOnly(true);                // Solo lectura para evitar edición
        chatArea->hide();                           // Oculto hasta que se conecte
        messageInput = new QLineEdit(this);         // Campo para escribir mensajes
        messageInput->hide();                       // Oculto hasta que se conecte
        sendButton = new QPushButton("Enviar", this); // Botón para enviar mensajes
        sendButton->hide();                         // Oculto hasta que se conecte
        userList = new QComboBox(this);             // Lista desplegable de usuarios
        userList->addItem("General");               // Canal general por defecto
        userList->hide();                           // Oculto hasta que se conecte

        rightLayout->addWidget(chatLabel);
        rightLayout->addWidget(chatArea);
        rightLayout->addWidget(userList);
        rightLayout->addWidget(messageInput);
        rightLayout->addWidget(sendButton);

        // Opcioness, general
        optionsButton->hide();                      // Oculto hasta que se conecte
        
        configLayout->addWidget(leftPanel);
        configLayout->addWidget(rightPanel);
        mainLayout ->addWidget(configPanel);
        mainLayout->addWidget(optionsButton);

        setLayout(mainLayout);

        // Configuración de conexiones entre señales y slots
        connect(connectButton, &QPushButton::clicked, this, &ChatClient::connectToServer);   // Conectar al hacer clic
        connect(&socket, &QWebSocket::connected, this, &ChatClient::onConnected);            // Manejar conexión exitosa
        connect(&socket, &QWebSocket::disconnected, this, &ChatClient::onDisconnected);      // Manejar desconexión
        connect(optionsButton, &QPushButton::clicked, this, &ChatClient::showOptionsDialog); // Mostrar opciones
        connect(userList, &QComboBox::currentTextChanged, this, &ChatClient::onUserSelected); // Manejar cambio de usuario seleccionado

        // Crear el manejador de mensajes (clase externa que procesa los mensajes)
        messageHandler = new MessageHandler(
            socket, 
            generalMessageInput, generalSendButton, generalChatArea,
            messageInput, sendButton, chatArea, 
            userList, statusDropdown, usernameInput, notificationLabel, notificationTimer,
            this
        );

        inactivityTimer = new QTimer(this);
        inactivityTimer->setInterval(40000); // 40 segundos
        inactivityTimer->setSingleShot(true); // Solo se activa una vez tras la inactividad
        connect(inactivityTimer, &QTimer::timeout, this, &ChatClient::setInactiveState);

        connect(generalMessageInput, &QLineEdit::textChanged, this, &ChatClient::resetInactivityTimer);
        connect(messageInput, &QLineEdit::textChanged, this, &ChatClient::resetInactivityTimer);
        connect(userList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChatClient::resetInactivityTimer);
        connect(statusDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChatClient::resetInactivityTimer);
    }

public slots:
    /**
     * @brief Establece conexión con el servidor de chat
     * 
     * Obtiene los datos de conexión de los campos de la UI y
     * intenta establecer una conexión WebSocket con el servidor.
     */
    void connectToServer() {
        QString host = hostInput->text();
        QString port = portInput->text();
        QString username = usernameInput->text();

        // Validación de campos obligatorios
        if (host.isEmpty() || port.isEmpty() || username.isEmpty()) {
            statusLabel->setText(" Todos los campos son obligatorios.");
            return;
        }

        // Formatear la URL del WebSocket con los parámetros
        // El formato es: ws://host:puerto?name=usuario
        QString url = QString("ws://%1:%2?name=%3").arg(host, port, username);
        statusLabel->setText("🔄 Conectando a " + url + "...");

        // Registro para depuración
        qDebug() << "Conectando a la URL: " << url;

        // Conectar las señales para manejar eventos de la conexión
        connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &ChatClient::onSocketError);

        // Iniciar la conexión WebSocket
        socket.open(QUrl(url));
    }

    /**
     * @brief Maneja errores de conexión del socket
     * @param error Tipo de error generado
     * 
     * Procesa diferentes tipos de errores de conexión y muestra
     * mensajes informativos al usuario.
     */
    void onSocketError(QAbstractSocket::SocketError error) {
        QString errorMessage;
        // Traducir códigos de error a mensajes descriptivos
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
        // Mostrar mensaje de error en la interfaz
        statusLabel->setText("❌ Error: " + errorMessage);
    }
    
    /**
     * @brief Maneja el evento de desconexión del servidor
     * 
     * Actualiza la UI para mostrar nuevamente la pantalla de inicio de sesión
     * cuando se pierde la conexión con el servidor.
     */
    void onDisconnected() {
        statusLabel->setText("Se ha desconectado de la sesión.");
    
        // Mostrar controles de conexión
        hostInput->show();
        portInput->show();
        usernameInput->show();
        connectButton->show();
        errorLabel->show();
    
        // Ocultar la interfaz de chat
        chatArea->hide();
        userList->hide();
        messageInput->hide();
        sendButton->hide();
        optionsButton->hide();
        generalChatLabel->hide();
        generalChatArea->hide();                    
        generalMessageInput->hide();               
        generalSendButton->hide(); 
        statusDropdown->hide();   
        chatLabel->hide(); 
    }

    /**
     * @brief Método alternativo para manejar errores WebSocket
     * @param error Tipo de error generado
     * 
     * Muestra el mensaje de error sin cambiar el estado de conexión.
     */
    void onWebSocketError(QAbstractSocket::SocketError error) {
        Q_UNUSED(error);
        
        // Obtener mensaje de error del socket
        QString errorMsg = socket.errorString();
        
        // Mostrar solo en la etiqueta de error
        errorLabel->setText("Error: " + errorMsg);
        qDebug() << "Error en WebSocket:" << errorMsg;
    }

    /**
     * @brief Maneja el evento de conexión exitosa
     * 
     * Actualiza la UI para mostrar la interfaz de chat cuando
     * se establece la conexión con el servidor.
     */
    void onConnected() {
        
        statusLabel->setText("Bienvenid@ " + usernameInput->text());
        statusDropdown->setEnabled(true);  // Habilitar selección de estado

        // Desconectar la selección de usuarios para no llamar a su historial inmediatamente
        disconnect(userList, &QComboBox::currentTextChanged, this, &ChatClient::onUserSelected);
        
        // Ocultar controles de conexión
        hostInput->hide();
        portInput->hide();
        usernameInput->hide();
        connectButton->hide();
        
        // Mostrar la interfaz de chat general
        generalChatLabel->show();
        generalChatArea->show();
        generalMessageInput->show();
        generalSendButton->show();

        // Solicitar historial de chat general
        generalChatArea->clear();  // Limpiar antes de mostrar los mensajes
        messageHandler->requestChatHistory("~"); // Cargar historial del canal

        // Mostrar la interfaz de chat personal
        chatLabel->show();
        chatArea->show();
        userList->show();
        messageInput->show();
        sendButton->show();
        optionsButton->show();
        statusDropdown->show();

        // Registrar nombre de usuario
        messageHandler->setActualUser(usernameInput->text());

        inactivityTimer->start(40000);

        // Reconectar cuando todo está listo
        QTimer::singleShot(500, this, [this]() {
            // Asegurarse de que no haya conexiones previas antes de conectar de nuevo
            connect(userList, &QComboBox::currentTextChanged, this, &ChatClient::onUserSelected);
        });
    }    

    /**
     * @brief Intenta reconectar con el servidor
     * 
     * Llamado por el temporizador de reconexión para intentar
     * restablecer la conexión con el servidor.
     */
    void attemptReconnect() {
        connectToServer();
    }

    /**
     * @brief Maneja el cambio de usuario seleccionado
     * 
     * Solicita el historial de chat del usuario/canal seleccionado.
     */
    void onUserSelected(const QString& selectedUser) {
        
        if (!selectedUser.isEmpty()) {
            chatArea->clear();  // Limpiar antes de mostrar los mensajes
            // Cargar historial del usuario/canal seleccionado
            messageHandler->requestChatHistory(selectedUser);
        }
    }

    void resetInactivityTimer() {
        inactivityTimer->start(40000); 
    }
    
    void setInactiveState() {
        if (messageHandler && !usernameInput->text().isEmpty()) {
            if (statusDropdown->currentIndex() == 2) {
                notificationLabel->setText("Sigues Inactivo");
                notificationLabel->show();
                notificationTimer->start(10000);
            } else {
                statusDropdown->setCurrentIndex(2); // Índice 2 = "Inactivo" 
            }
        }
    }

    /**
     * @brief Muestra el diálogo de opciones
     * 
     * Crea y configura un diálogo de opciones que permite gestionar
     * información de los usuarios conectados.
     */
    void showOptionsDialog() {
        OptionsDialog *dialog = new OptionsDialog(this);
        
        // Pasar la lista de usuarios al diálogo
        dialog->setUserList(userList);
        
        // Configurar para auto-destrucción al cerrar
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        
        // Configurar función para solicitar información de usuarios
        dialog->setRequestInfoFunction([this](const QString& username) {
            messageHandler->requestUserInfo(username);
        });
        
        // Configurar callback para recibir información de usuarios
        messageHandler->setUserInfoCallback([dialog](const QString& username, int status) {
            dialog->addUserInfo(username, status);
        });
        
        // Limpiar callback cuando se cierre el diálogo
        connect(dialog, &QDialog::finished, this, [this]() {
            messageHandler->setUserInfoCallback(nullptr);
        });
        
        // Mostrar diálogo como no-modal (permite interactuar con la ventana principal)
        dialog->show();
    }

private:
    QWebSocket socket;              // Socket para la comunicación WebSocket
    QLabel *statusLabel;            // Etiqueta para mostrar el estado de la conexión
    QLabel *errorLabel;             // Etiqueta para mostrar mensajes de error
    QLineEdit *hostInput;           // Campo para dirección del servidor
    QLineEdit *portInput;           // Campo para puerto del servidor
    QLineEdit *usernameInput;       // Campo para nombre de usuario
    QPushButton *connectButton;     // Botón para conectar
    QPushButton *optionsButton;     // Botón para mostrar opciones
    QLabel *generalChatLabel;       // Etiqueta para reconocer el área de chat general
    QLabel *chatLabel;              // Etiqueta para reconocer el área de chat
    QTextEdit *generalChatArea;            // Área de visualización de mensajes en chat general
    QLineEdit *generalMessageInput;        // Campo para escribir mensajes en chat general
    QPushButton *generalSendButton;        // Botón para enviar mensajes en chat general
    QTextEdit *chatArea;            // Área de visualización de mensajes
    QLineEdit *messageInput;        // Campo para escribir mensajes
    QPushButton *sendButton;        // Botón para enviar mensajes
    QComboBox *userList;            // Lista de usuarios/canales
    QComboBox *statusDropdown;      // Selector de estado del usuario
    MessageHandler *messageHandler; // Manejador de mensajes
    QLabel *notificationLabel;
    QTimer *notificationTimer;
    QTimer *inactivityTimer;
};

/**
 * @brief Punto de entrada principal de la aplicación
 * 
 * Inicializa la aplicación Qt y crea la ventana principal del cliente de chat.
 */
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);   // Crear aplicación Qt
    ChatClient client;              // Crear la ventana del cliente
    client.show();                  // Mostrar la ventana
    return app.exec();              // Iniciar el bucle de eventos
}