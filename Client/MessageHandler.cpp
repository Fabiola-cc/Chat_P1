#include "MessageHandler.h"
#include <iostream>
#include <unordered_map>
#include <string>
#include <stdexcept>

using namespace std;
unordered_map<string, vector<pair<string, string>>> localChatHistory; // clave: ID del chat

/**
 * @brief Constructor de la clase MessageHandler
 * 
 * @param socket WebSocket utilizado para la comunicación con el servidor
 * @param generalInput Campo de entrada para mensajes de texto del chat general
 * @param generalButton Botón para enviar mensajes al chat general
 * @param generalChatArea Área de texto donde se muestran los mensajes del chat general
 * @param input Campo de entrada para mensajes de texto del chat personal
 * @param button Botón para enviar mensajes al chat personal
 * @param chatArea Área de texto donde se muestran los mensajes del chat personal
 * @param userList Lista desplegable de usuarios para el chat personal
 * @param stateList Lista desplegable para seleccionar el estado del usuario
 * @param usernameInput Campo con el nombre del usuario actual
 * @param parent Objeto padre para la gestión de memoria (modelo Qt parent-child)
 */
MessageHandler::MessageHandler(QWebSocket& socket, 
    QLineEdit* generalInput, QPushButton* generalButton, QTextEdit* generalChatArea,
    QLineEdit* input, QPushButton* button, QTextEdit* chatArea, 
    QComboBox* userList, QComboBox* stateList, QLineEdit* usernameInput,  QLabel* notificationLabel, QTimer* notificationTimer,
    QObject* parent)
    : QObject(parent), socket(socket), 
    generalMessageInput(generalInput), generalSendButton(generalButton), generalChatArea(generalChatArea),
    messageInput(input), sendButton(button), chatArea(chatArea), 
    userList(userList), stateList(stateList), usernameInput(usernameInput), notificationLabel(notificationLabel), notificationTimer(notificationTimer),
    m_userInfoCallback(nullptr) { 

    actualUser = ""; // Espacio para registrar el nombre del usuario actual
    pendingHistoryRequests; // Bandera de quién está solicitando el historial de chat

    // Conectar señales y slots para el chat personal
    connect(sendButton, &QPushButton::clicked, this, &MessageHandler::sendMessage);
    connect(&socket, &QWebSocket::textMessageReceived, this, &MessageHandler::receiveMessage);

    // Conectar señales y slots para el chat general
    connect(generalSendButton, &QPushButton::clicked, this, &MessageHandler::sendGeneralMessage);
    connect(&socket, &QWebSocket::binaryMessageReceived, this, &MessageHandler::receiveBinaryMessage);

    // Conectar cambios de estado
    connect(stateList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MessageHandler::onStateChanged);

}

/**
 * Guarda el nombre de usuario actual
 */
void MessageHandler::setActualUser(const QString& username) {
    actualUser = username.trimmed();
}


/**
 * @brief Convierte un código numérico de estado a su representación textual
 * 
 * @param status Código numérico del estado
 * @return string Descripción textual del estado
 */
string get_status_string(int status) {
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Activo";
        case 2: return "Ocupado";
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}

/**
 * @brief Convierte un código numérico de error a su descripción
 * 
 * @param error Código numérico del error
 * @return string Descripción textual del error
 */
string get_error_string(int error) {
    switch (error) {
        case 1: return "El usuario que intentas obtener no existe.";
        case 2: return "El estatus enviado es inválido.";
        case 3: return "¡El mensaje está vacío!";
        case 4: return "El mensaje fue enviado a un usuario con estatus desconectado";
        default: return "Desconocido";
    }
}

/**
 * @brief Establece la función callback para manejar información de usuario recibida
 * 
 * @param callback Función que será llamada cuando se reciba información de un usuario
 */
void MessageHandler::setUserInfoCallback(function<void(const QString&, int)> callback) {
    m_userInfoCallback = callback;
}

/**
 * @brief Solicita información sobre un usuario específico al servidor
 * 
 * @param username Nombre del usuario del que se desea obtener información
 */
void MessageHandler::requestUserInfo(const QString& username) {
    if (username.isEmpty()) return;  // Validar entrada

    // Crear mensaje binario para solicitar información
    // Formato: [Tipo=2][LongitudNombre][Nombre]
    QByteArray request;
    request.append(static_cast<char>(2));  // Tipo 2: Obtener info de usuario
    request.append(static_cast<char>(username.length()));  // Longitud del nombre
    request.append(username.toUtf8());  // Nombre en UTF-8

    socket.sendBinaryMessage(request);  // Enviar solicitud al servidor
}

void MessageHandler::setUserListReceivedCallback(std::function<void(const std::unordered_map<std::string, std::string>&)> callback) {
    m_userListReceivedCallback = callback;
}

void MessageHandler::requestUsersList() {
    if (socket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << "⚠️ Cannot request user list: WebSocket not connected";
        return;
    }
    
    // Crear mensaje binario para solicitar la lista de usuarios
    // Formato: [Tipo=1]
    QByteArray request;
    request.append(static_cast<char>(1));  // Tipo 1: Obtener lista de usuarios

    qDebug()<<"Pidiendo lista de usuarios: "<<request;
    
    socket.sendBinaryMessage(request);  // Enviar solicitud al servidor
}
/**
 * @brief Solicita el historial de conversación de un chat específico
 * 
 * @param chatName Nombre del chat (un usuario o "~" para el chat general)
 */
void MessageHandler::requestChatHistory(const QString& chatName) {
    pendingHistoryRequests.push(chatName);
    if (chatName.isEmpty()) return;  // Validar entrada

    // Crear mensaje binario para solicitar historial
    // Formato: [Tipo=5][LongitudNombre][Nombre]
    QByteArray request;
    request.append(static_cast<char>(5));  // Tipo 5: Solicitar historial
    request.append(static_cast<char>(chatName.length()));  // Longitud del nombre
    request.append(chatName.toUtf8());  // Nombre en UTF-8

    qDebug()<<"Pidiendo chat history: "<<request;

    socket.sendBinaryMessage(request);  // Enviar solicitud al servidor
}

/**
 * @brief Solicita cambiar el estado de un usuario
 * 
 * @param username Nombre del usuario cuyo estado se va a cambiar
 * @param newStatus Nuevo estado (1=Activo, 2=Ocupado, 3=Inactivo)
 */
void MessageHandler::requestChangeState(const QString& username, uint8_t newStatus) {
    if (username.isEmpty()) return;  // Validar entrada

    // Crear mensaje binario para cambiar estado
    // Formato: [Tipo=3][LongitudNombre][Nombre][NuevoEstado]
    QByteArray request;
    request.append(static_cast<char>(3));  // Tipo 3: Cambiar estado
    request.append(static_cast<char>(username.length()));  // Longitud del nombre
    request.append(username.toUtf8());  // Nombre en UTF-8
    request.append(static_cast<char>(newStatus));  // Nuevo estado

    qDebug()<<"Pidiendo state change: "<<request;

    socket.sendBinaryMessage(request);  // Enviar solicitud al servidor
}

/**
 * @brief Maneja el cambio de estado seleccionado en la interfaz
 * 
 * @param index Índice del nuevo estado seleccionado
 */
void MessageHandler::onStateChanged(int index) {
    if (!stateList) return;  // Validar que existe el control

    // Obtener el valor numérico asociado al índice seleccionado
    uint8_t newStatus = static_cast<uint8_t>(stateList->itemData(index).toInt());

    // Enviar solicitud de cambio de estado si hay un usuario válido
    if (!actualUser.isEmpty()) {
        requestChangeState(actualUser, newStatus);
    }
}

/**
 * @brief Envía un mensaje de chat
 * 
 * Obtiene el texto del campo de entrada y el destinatario seleccionado,
 * construye el mensaje y lo envía al servidor.
 */
void MessageHandler::sendMessage() {
    // Obtener y validar el mensaje
    QString message = messageInput->text().trimmed();
    if (message.isEmpty()) {
        chatArea->append("!! Mensaje vacío");
        return;
    }
    if (message.length() > 255) {
        chatArea->append("!! Mensaje inválido (demasiado largo)");
        return;
    }

    // Determinar el destinatario
    QString recipient = userList->currentText();

    if (userStates[recipient.toStdString()] == "Desconectado")
    {
        chatArea->append("No puedes mandar mensaje, este usuario se desconectó");
        notificationLabel->setText(recipient + " está desconectado");
        notificationLabel->show();
        notificationTimer->start(5000);
        return;
    }
    
    // Construir y enviar el mensaje
    QByteArray formattedMessage = buildMessage(4, recipient, message);  // Tipo 4: mensaje de chat
    socket.sendBinaryMessage(formattedMessage);

    messageInput->clear();  // Limpiar campo de entrada
}

/**
 * @brief Envía un mensaje de chat general
 * 
 * Obtiene el texto del campo de entrada y el destinatario seleccionado,
 * construye el mensaje y lo envía al servidor.
 */
void MessageHandler::sendGeneralMessage() {
    // Verificar que hay texto para enviar
    QString message = generalMessageInput->text().trimmed();

    if (message.isEmpty()) {
        generalChatArea->append("!! Mensaje vacío");
        return;
    }
    if (message.length() > 255) {
        generalChatArea->append("!! Mensaje inválido (demasiado largo)");
        return;
    }

    // Determinar el destinatario
    QString recipient = "~";

    // Construir y enviar el mensaje
    QByteArray formattedMessage = buildMessage(4, recipient, message);  // Tipo 4: mensaje de chat
    
    socket.sendBinaryMessage(formattedMessage);

    // Limpiar el campo de entrada después de enviar
    generalMessageInput->clear();
}

/** 
 * Genera una clave única para cada conversación.
 * 
 * @param user2 Receptor en la conversación (QString).
 * @return QString con la clave única de la conversación.
 */
QString MessageHandler::get_chat_id(const QString& user2) {
    QString user1 = actualUser;
    if (user1 < user2) {
        return user1 + "-" + user2;  // Orden lexicográfico
    } else {
        return user2 + "-" + user1;
    }
}

/**
 * Guarda un mensaje específico en el chat establecido
 * 
 * @param sender usuario que mandó el mensaje
 * @param message mensaje a guardar
 */
void MessageHandler::storeMessage(const QString& sender, const QString& message) {
    // Obtener el chat_id en formato QString o mantenerlo si es "~"
    QString chat_id_qt = sender != "~" ? get_chat_id(sender) : sender;
    std::string chat_id_std = chat_id_qt.toStdString();

    // Verificar si el mensaje ya está en el historial local
    auto& chatHistory = localChatHistory[chat_id_std];
    if (std::find(chatHistory.begin(), chatHistory.end(),
                  std::make_pair(sender.toStdString(), message.toStdString())) == chatHistory.end()) {
        // Agregar mensaje al historial local
        chatHistory.emplace_back(sender.toStdString(), message.toStdString());
    }
}

/**
 * Muestra todos los mensajes de un chat específico
 * 
 * @param user2 Segunda persona en conversación (puede ser el chat general)
 */
void MessageHandler::showChatMessages(const QString& user2) {
    if (stateList->currentText() == "Ocupado") return; // No mostrar a un usuario ocupado
    // Determinar si es chat general o personal
    bool isGeneralChat = (user2 == "~");

    // Seleccionar el área de chat adecuada
    QTextEdit* targetChatArea = isGeneralChat ? generalChatArea : chatArea;
    targetChatArea->clear(); //Limpiar antes de mostrar los mensajes
    string chat_id = isGeneralChat ? user2.toStdString() : get_chat_id(user2).toStdString();

    // Verificar si existe historial para el chat dado
    auto it = localChatHistory.find(chat_id);
    if (it != localChatHistory.end()) {
        for (const auto& pair : it->second) {
            const auto& sender = pair.first;
            const auto& content = pair.second;
            string displaySender = actualUser.toStdString() == sender? "Tú" : sender;
            targetChatArea->append(QString::fromStdString(displaySender) + ": " + QString::fromStdString(content));
        }
    }
}

/**
 * @brief Construye un mensaje binario con el formato del protocolo
 * 
 * @param type Tipo de mensaje (código de operación)
 * @param param1 Primer parámetro (generalmente destinatario)
 * @param param2 Segundo parámetro (generalmente contenido)
 * @return QByteArray Mensaje binario formateado según el protocolo
 */
QByteArray MessageHandler::buildMessage(quint8 type, const QString& param1, const QString& param2) {
    QByteArray message;
    // Añadir tipo de mensaje
    message.append(static_cast<char>(type));

    // Añadir primer parámetro si existe
    if (!param1.isEmpty()) {
        message.append(static_cast<char>(param1.length()));  // Longitud
        message.append(param1.toUtf8());  // Datos
    }

    // Añadir segundo parámetro si existe
    if (!param2.isEmpty()) {
        message.append(static_cast<char>(param2.length()));  // Longitud
        message.append(param2.toUtf8());  // Datos
    }
    qDebug() << "mensaje:" << QString::fromUtf8(message);
    return message;
}

/**
 * @brief Procesa los mensajes recibidos del servidor
 * 
 * @param message Mensaje recibido (en formato binario)
 * 
 * Esta función analiza el tipo de mensaje y extrae la información
 * relevante para actualizar la interfaz de usuario.
 */
void MessageHandler::receiveMessage(const QString& message) {
    
    QByteArray data = message.toUtf8();
    qDebug()<<"RECIBIDO DESPUES "<<data;

    if (data.isEmpty()) return;  // Validar entrada

    quint8 messageType = static_cast<quint8>(data[0]);  // Obtener tipo de mensaje

    qDebug()<<"TIPO MENSAJE"<<messageType;

    // Procesar según el tipo de mensaje
    if (messageType == 50) { // ERROR
        quint8 errorType = static_cast<quint8>(data[1]);
        cerr << "⚠️ " + get_error_string(errorType) << endl;  // Log en consola
    }
    else if (messageType == 51) {  // Lista de usuarios con estados
        userList->clear();  // Limpiar lista actual
        quint8 numUsers = static_cast<quint8>(data[1]);  // Número de usuarios
        int pos = 2;  // Posición para leer datos

        // Procesar cada usuario en la lista
        for (quint8 i = 0; i < numUsers; i++) {
            // Leer nombre de usuario
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;  // Avanzar posición

            // Leer estado
            quint8 status = static_cast<quint8>(data[pos]);
            pos += 1;  // Avanzar posición

            // Convertir status numérico a string
            string user_status = get_status_string(status);

            // Guardar en la estructura de datos
            this-> userStates[username.toStdString()] = user_status;

            if (username != actualUser && userList->findText(username) == -1) {
                userList->addItem(username);  // Añadir solo si no está en la lista
            }
            

            // Actualizar estado si es el usuario actual
            if (username == userList->currentText()) {
                int stateIndex = stateList->findData(status);
                if (stateIndex != -1) {
                    stateList->setCurrentIndex(stateIndex);
                }
            }
        }
        
        if (m_userListReceivedCallback) {
            m_userListReceivedCallback(userStates);
        }
    } 
    else if (messageType == 52) {  // Información de usuario
        quint8 success = static_cast<quint8>(data[1]);  // Éxito/fracaso

        if (success) {
            // Extraer información del usuario
            quint8 usernameLen = static_cast<quint8>(data[2]);
            QString username = QString::fromUtf8(data.mid(3, usernameLen));
            quint8 status = static_cast<quint8>(data[3 + usernameLen]);

            // Llamar al callback si está configurado
            if (m_userInfoCallback) {
                m_userInfoCallback(username, status);
            }
        } else {
            generalChatArea->append("No se encontró información del usuario solicitado");
        }
    }
    else if (messageType == 53) {  // Nuevo usuario conectado
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        notificationLabel->setText(username + " se ha registrado!");
        notificationLabel->show();
        notificationTimer->start(5000);
        userList->addItem(username);  // Añadir a la lista de usuarios
        userStates[username.toStdString()] = get_status_string(1); // Añadir su estado actual
    }
    else if (messageType == 54) {  // Cambio de estado de usuario
        
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        quint8 newStatus = static_cast<quint8>(data[2 + usernameLen]);
        notificationLabel->setText(username + " ha cambiado su estado a " + 
                        QString::fromStdString(get_status_string(newStatus)));
        notificationLabel->show();
        notificationTimer->start(5000);
        
        string last_status = userStates[username.toStdString()];
        userStates[username.toStdString()] = get_status_string(newStatus);

        if (actualUser != username) return; // Solo actúa si el usuario actual es el afectado
        
        if (last_status == "Ocupado" && newStatus == 1) {  // Recuperar los mensajes si se cambia a activo
            requestChatHistory("~");
            requestChatHistory(userList->currentText());
        }
    }
    else if (messageType == 55) {  // Mensaje normal de chat

        // Extraer remitente
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));

        // Extraer contenido del mensaje
        quint8 messageLen = static_cast<quint8>(data[2 + usernameLen]);
        QString content = QString::fromUtf8(data.mid(3 + usernameLen, messageLen));

        if (stateList->currentText() == "Ocupado") {
            storeMessage(username, content);
            return;
        }

        // Definir nombre de usuario en la UI
        QString displayUsername = (actualUser == username) ? "Tú" : username;

        // Si es el chat general, solo mostramos el contenido
        if (displayUsername == "~") {
            generalChatArea->append(content);
            return;
        }

        // Determinar el chat en el que estamos
        QString actualChat = userList->currentText();

        if (displayUsername == "Tú" || displayUsername == actualChat) {
            chatArea->append(displayUsername + ": " + content);
        } else {
            notificationLabel->setText("Recibiste un nuevo mensaje de: " + username);
            notificationLabel->show();
            notificationTimer->start(5000);
        }

    } 
    else if (messageType == 56) {  // Historial de chat recibido
        quint8 numMessages = static_cast<quint8>(data[1]);  // Número de mensajes
        int pos = 2;  // Posición para leer datos

        if (pendingHistoryRequests.empty()) return;
        
        QString requestedHistory = pendingHistoryRequests.front();
        pendingHistoryRequests.pop();
        
        localChatHistory.clear();  // Eliminar mensajes previos del historial
        for (quint8 i = 0; i < numMessages; i++) {

            // Extraer remitente
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;  // Avanzar posición

            // Extraer contenido
            quint8 messageLen = static_cast<quint8>(data[pos]);
            QString content = QString::fromUtf8(data.mid(pos + 1, messageLen));
            pos += 1 + messageLen;  // Avanzar posición

            // Construir la clave del chat para el historial
            QString chat_id_qt = requestedHistory.toStdString() != "~" ? get_chat_id(requestedHistory) : requestedHistory;;
            // Convertir a string solo para acceder a unordered_map
            string chat_id_std = chat_id_qt.toStdString();

            // Verificar si el mensaje ya está en el historial local
            auto& chatHistory = localChatHistory[chat_id_std];
            // Agregar mensaje al historial local
            chatHistory.emplace_back(username.toStdString(), content.toStdString());
        }
        
        showChatMessages(requestedHistory);
    }    
    else {
        // Tipo de mensaje desconocido
        qDebug() << "MENSAJE NO CONOCIDO" <<  messageType;
        chatArea->append("!! Mensaje desconocido recibido.");
    }
}

void MessageHandler::receiveBinaryMessage(const QByteArray& data) {
    qDebug() << "MENSAJE BINARIO RECIBIDO:" << data;
    
    if (data.isEmpty()) return;
    
    quint8 messageType = static_cast<quint8>(data[0]);
    qDebug() << "TIPO MENSAJE BINARIO:" << messageType;
    
    // Convertir los datos binarios a QString y utilizar la función existente
    QString message = QString::fromUtf8(data);
    receiveMessage(message);
}