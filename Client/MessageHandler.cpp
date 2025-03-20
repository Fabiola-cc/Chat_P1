#include "MessageHandler.h"

MessageHandler::MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, QTextEdit* chatArea, QObject* parent)
    : QObject(parent), socket(socket), messageInput(input), sendButton(button), chatArea(chatArea) {
    
    connect(sendButton, &QPushButton::clicked, this, &MessageHandler::sendMessage);
    connect(&socket, &QWebSocket::textMessageReceived, this, &MessageHandler::receiveMessage);
}

void MessageHandler::requestChatHistory(const QString& chatName) {
    if (chatName.isEmpty()) return;

    QByteArray request;
    request.append(static_cast<char>(5));  // Código para obtener historial
    request.append(static_cast<char>(chatName.length()));
    request.append(chatName.toUtf8());

    socket.sendBinaryMessage(request);
}

void MessageHandler::sendMessage() {
    QString message = messageInput->text().trimmed();
    if (message.isEmpty() || message.length() > 255) {
        chatArea->append("!! Mensaje inválido (vacío o muy largo)");
        return;
    }

    QByteArray formattedMessage = buildMessage(4, "~", message);  // Código 4 = Mensaje al chat general
    socket.sendBinaryMessage(formattedMessage);

    messageInput->clear();
}

// 📌 Construye un mensaje binario según el protocolo
QByteArray MessageHandler::buildMessage(quint8 type, const QString& param1, const QString& param2) {
    QByteArray message;
    message.append(static_cast<char>(type));  // Código del mensaje

    if (!param1.isEmpty()) {
        message.append(static_cast<char>(param1.length()));  // Longitud del primer campo
        message.append(param1.toUtf8());  // Contenido del primer campo
    }

    if (!param2.isEmpty()) {
        message.append(static_cast<char>(param2.length()));  // Longitud del segundo campo
        message.append(param2.toUtf8());  // Contenido del segundo campo
    }

    return message;
}

void MessageHandler::receiveMessage(const QString& message) {
    QByteArray data = message.toUtf8();
    if (data.isEmpty()) return;

    quint8 messageType = static_cast<quint8>(data[0]);

    if (messageType == 55) {  // 📩 Mensaje recibido normal
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        quint8 messageLen = static_cast<quint8>(data[2 + usernameLen]);
        QString content = QString::fromUtf8(data.mid(3 + usernameLen, messageLen));

        chatArea->append("💬 " + username + ": " + content);
    } 
    else if (messageType == 56) {  // 📜 Respuesta de historial de chat
        chatArea->clear();  // 🔹 Limpiar chat antes de mostrar historial

        quint8 numMessages = static_cast<quint8>(data[1]);
        int pos = 2;
        
        for (quint8 i = 0; i < numMessages; i++) {
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;

            quint8 messageLen = static_cast<quint8>(data[pos]);
            QString content = QString::fromUtf8(data.mid(pos + 1, messageLen));
            pos += 1 + messageLen;

            chatArea->append("📜 " + username + ": " + content);
        }
    } 
    else {
        chatArea->append("!! Mensaje desconocido recibido.");
    }
}
