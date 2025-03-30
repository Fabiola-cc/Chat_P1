#include "MessageHandler.h"
#include <iostream>
#include <stdexcept>

using namespace std;

/**
 * @brief Constructor de la clase MessageHandler
 * 
 * @param socket WebSocket utilizado para la comunicación con el servidor
 * @param input Campo de entrada para mensajes de texto
 * @param button Botón para enviar mensajes
 * @param chatArea Área de texto donde se muestran los mensajes
 * @param userList Lista desplegable de usuarios
 * @param stateList Lista desplegable para seleccionar el estado del usuario
 * @param usernameInput Campo con el nombre del usuario actual
 * @param parent Objeto padre para la gestión de memoria (modelo Qt parent-child)
 */
MessageHandler::MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, 
                               QTextEdit* chatArea, QComboBox* userList, QComboBox* stateList,  QLineEdit* usernameInput,  QObject* parent)
    : QObject(parent), socket(socket), messageInput(input), sendButton(button), 
      chatArea(chatArea), userList(userList), stateList(stateList), usernameInput(usernameInput),  m_userInfoCallback(nullptr) {  
    
    // Conectar señales y slots
    connect(sendButton, &QPushButton::clicked, this, &MessageHandler::sendMessage);         // Enviar mensaje al hacer clic
    connect(&socket, &QWebSocket::textMessageReceived, this, &MessageHandler::receiveMessage); // Procesar mensajes recibidos
    connect(stateList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MessageHandler::onStateChanged);  // Conectar cambios de estado
}

/**
 * @brief Convierte un código numérico de estado a su representación textual
 * 
 * @param status Código numérico del estado
 * @return std::string Descripción textual del estado
 */
std::string get_status_string(int status) {
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
 * @return std::string Descripción textual del error
 */
std::string get_error_string(int error) {
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
void MessageHandler::setUserInfoCallback(std::function<void(const QString&, int)> callback) {
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

/**
 * @brief Solicita el historial de conversación de un chat específico
 * 
 * @param chatName Nombre del chat (un usuario o "~" para el chat general)
 */
void MessageHandler::requestChatHistory(const QString& chatName) {
    if (chatName.isEmpty()) return;  // Validar entrada

    // Crear mensaje binario para solicitar historial
    // Formato: [Tipo=5][LongitudNombre][Nombre]
    QByteArray request;
    request.append(static_cast<char>(5));  // Tipo 5: Solicitar historial
    request.append(static_cast<char>(chatName.length()));  // Longitud del nombre
    request.append(chatName.toUtf8());  // Nombre en UTF-8

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
    QString username = usernameInput->text().trimmed();  // Obtener nombre de usuario actual

    // Enviar solicitud de cambio de estado si hay un usuario válido
    if (!username.isEmpty()) {
        requestChangeState(username, newStatus);
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

    // Determinar el destinatario (~ para chat general)
    QString recipient = userList->currentText();
    if (recipient == "General") recipient = "~";

    // Construir y enviar el mensaje
    QByteArray formattedMessage = buildMessage(4, recipient, message);  // Tipo 4: mensaje de chat
    socket.sendBinaryMessage(formattedMessage);

    messageInput->clear();  // Limpiar campo de entrada
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
    if (data.isEmpty()) return;  // Validar entrada

    quint8 messageType = static_cast<quint8>(data[0]);  // Obtener tipo de mensaje

    // Procesar según el tipo de mensaje
    if (messageType == 50) { // ERROR
        quint8 errorType = static_cast<quint8>(data[1]);
        cerr << "⚠️ " + get_error_string(errorType) << endl;  // Log en consola
        chatArea->append(QString::fromStdString(get_error_string(errorType)));  // Mostrar en chat
    }
    else if (messageType == 51) {  // Lista de usuarios con estados
        userList->clear();  // Limpiar lista actual
        quint8 numUsers = static_cast<quint8>(data[1]);  // Número de usuarios
        int pos = 2;  // Posición para leer datos
        userList->addItem("General");  // Añadir chat general

        // Procesar cada usuario en la lista
        for (quint8 i = 0; i < numUsers; i++) {
            // Leer nombre de usuario
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;  // Avanzar posición

            // Leer estado
            quint8 status = static_cast<quint8>(data[pos]);
            pos += 1;  // Avanzar posición

            userList->addItem(username);  // Añadir a la lista
            
            // Actualizar estado si es el usuario actual
            if (username == userList->currentText()) {
                int stateIndex = stateList->findData(status);
                if (stateIndex != -1) {
                    stateList->setCurrentIndex(stateIndex);
                }
            }
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
            
            // Mostrar información en el chat
            chatArea->append("Información de usuario: " + username + 
                             " - Estado: " + QString::fromStdString(get_status_string(status)));
        } else {
            chatArea->append("No se encontró información del usuario solicitado");
        }
    }
    else if (messageType == 53) {  // Nuevo usuario conectado
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        userList->addItem(username);  // Añadir a la lista de usuarios
    }
    else if (messageType == 54) {  // Cambio de estado de usuario
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        quint8 newStatus = static_cast<quint8>(data[2 + usernameLen]);
        
        // Mostrar notificación del cambio de estado
        chatArea->append("**" + username + " ha cambiado su estado a " + 
                        QString::fromStdString(get_status_string(newStatus)) + "**");
    }
    else if (messageType == 55) {  // Mensaje normal de chat
        // Extraer remitente
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        
        // Extraer contenido del mensaje
        quint8 messageLen = static_cast<quint8>(data[2 + usernameLen]);
        QString content = QString::fromUtf8(data.mid(3 + usernameLen, messageLen));
        
        // Mostrar mensaje en el área de chat
        chatArea->append(username + ": " + content);
    } 
    else if (messageType == 56) {  // Historial de chat
        chatArea->clear();  // Limpiar mensajes anteriores
        quint8 numMessages = static_cast<quint8>(data[1]);  // Número de mensajes
        int pos = 2;  // Posición para leer datos
        
        // Procesar cada mensaje del historial
        for (quint8 i = 0; i < numMessages; i++) {
            // Extraer remitente
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;  // Avanzar posición

            // Extraer contenido
            quint8 messageLen = static_cast<quint8>(data[pos]);
            QString content = QString::fromUtf8(data.mid(pos + 1, messageLen));
            pos += 1 + messageLen;  // Avanzar posición

            // Mostrar mensaje en el área de chat
            chatArea->append(username + ": " + content);
        }
    } 
    else {
        // Tipo de mensaje desconocido
        chatArea->append("!! Mensaje desconocido recibido.");
    }
}