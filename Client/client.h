#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

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
#include "MessageHandler.h"

class ChatClient : public QWidget {
    Q_OBJECT

public:
    ChatClient(QWidget *parent = nullptr);

private:
    QWebSocket socket;
    QLineEdit *hostInput, *portInput, *usernameInput, *messageInput;
    QPushButton *connectButton, *sendButton;
    QLabel *statusLabel;
    QTextEdit *chatArea;
    QComboBox *userList;  // 🔹 Nuevo dropdown para seleccionar usuarios
    QTimer *reconnectTimer;
    MessageHandler *messageHandler;

private slots:
    void onUserSelected();  // 🔹 Nueva función para cargar historial al seleccionar usuario
};

#endif // ChatClient_H
