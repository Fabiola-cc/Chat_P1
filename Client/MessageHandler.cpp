#include "MessageHandler.h"
#include <iostream>


MessageHandler::MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, 
                               QTextEdit* chatArea, QComboBox* userList, QComboBox* stateList,  QLineEdit* usernameInput,  QObject* parent)
    : QObject(parent), socket(socket), messageInput(input), sendButton(button), 
      chatArea(chatArea), userList(userList), stateList(stateList), usernameInput(usernameInput) {  
    
    connect(sendButton, &QPushButton::clicked, this, &MessageHandler::sendMessage);
    connect(&socket, &QWebSocket::textMessageReceived, this, &MessageHandler::receiveMessage);
    connect(stateList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MessageHandler::onStateChanged);  // 🔹 Conectar cambios de estado
}

std::string get_status_string(int status) {
    switch (status) {
        case 0: return "Desconectado";
        case 1: return "Activo";
        case 2: return "Ocupado";
        case 3: return "Inactivo";
        default: return "Desconocido";
    }
}

void MessageHandler::requestChatHistory(const QString& chatName) {
    if (chatName.isEmpty()) return;

    QByteArray request;
    request.append(static_cast<char>(5));  
    request.append(static_cast<char>(chatName.length()));
    request.append(chatName.toUtf8());

    socket.sendBinaryMessage(request);
}

void MessageHandler::requestChangeState(const QString& username, uint8_t newStatus) {
    if (username.isEmpty()) return;

    QByteArray request;
    request.append(static_cast<char>(3));  
    request.append(static_cast<char>(username.length()));
    request.append(username.toUtf8());
    request.append(static_cast<char>(newStatus));

    socket.sendBinaryMessage(request);
}

void MessageHandler::onStateChanged(int index) {
    if (!stateList) return;

    uint8_t newStatus = static_cast<uint8_t>(stateList->itemData(index).toInt());
    QString username = usernameInput->text().trimmed();  

    if (!username.isEmpty()) {
        requestChangeState(username, newStatus);
    }
}

void MessageHandler::sendMessage() {
    QString message = messageInput->text().trimmed();
    if (message.isEmpty() || message.length() > 255) {
        chatArea->append("!! Mensaje inválido (vacío o muy largo)");
        return;
    }

    QString recipient = userList->currentText();
    if (recipient == "General") recipient = "~";

    QByteArray formattedMessage = buildMessage(4, recipient, message);
    socket.sendBinaryMessage(formattedMessage);

    messageInput->clear();
}

QByteArray MessageHandler::buildMessage(quint8 type, const QString& param1, const QString& param2) {
    QByteArray message;
    message.append(static_cast<char>(type));

    if (!param1.isEmpty()) {
        message.append(static_cast<char>(param1.length()));
        message.append(param1.toUtf8());
    }

    if (!param2.isEmpty()) {
        message.append(static_cast<char>(param2.length()));
        message.append(param2.toUtf8());
    }

    return message;
}

void MessageHandler::receiveMessage(const QString& message) {
    QByteArray data = message.toUtf8();
    if (data.isEmpty()) return;

    quint8 messageType = static_cast<quint8>(data[0]);

    if (messageType == 51) {  // Lista de usuarios con estados
        userList->clear();
        quint8 numUsers = static_cast<quint8>(data[1]);
        int pos = 2;
        userList->addItem("General");

        for (quint8 i = 0; i < numUsers; i++) {
            quint8 usernameLen = static_cast<quint8>(data[pos]);
            QString username = QString::fromUtf8(data.mid(pos + 1, usernameLen));
            pos += 1 + usernameLen;

            quint8 status = static_cast<quint8>(data[pos]);
            pos += 1;

            userList->addItem(username);
            
            // 🔹 Si este usuario es el actual, actualizar su estado en el combo
            if (username == userList->currentText()) {
                int stateIndex = stateList->findData(status);
                if (stateIndex != -1) {
                    stateList->setCurrentIndex(stateIndex);
                }
            }
        }
    } 
    //else if (messageType == 52) {
      // break;
    //}
    else if (messageType == 53) {
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        userList->addItem(username);
    }
    else if (messageType == 54) {
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        quint8 newStatus = static_cast<quint8>(data[2 + usernameLen]);
        
        chatArea->append("**" + username + " ha cambiado su estado a " + QString::fromStdString(get_status_string(newStatus)) + "**");
    }
    else if (messageType == 55) {  // Mensaje normal
        quint8 usernameLen = static_cast<quint8>(data[1]);
        QString username = QString::fromUtf8(data.mid(2, usernameLen));
        quint8 messageLen = static_cast<quint8>(data[2 + usernameLen]);
        QString content = QString::fromUtf8(data.mid(3 + usernameLen, messageLen));
        
        chatArea->append("💬 " + username + ": " + content);
    } 
    else if (messageType == 56) {  // Historial de chat
        chatArea->clear();
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
