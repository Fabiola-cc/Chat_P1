#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QWebSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <functional> // Para usar std::function

class MessageHandler : public QObject {
    Q_OBJECT

public:
    explicit MessageHandler(QWebSocket& socket, 
        QLineEdit* generalInput, QPushButton* generalButton, QTextEdit* generalChatArea,
        QLineEdit* input, QPushButton* button, QTextEdit* chatArea, 
        QComboBox* userList, QComboBox* stateList, QLineEdit* usernameInput,
        QObject* parent = nullptr);

    void requestChatHistory(const QString& chatName);
    void requestChangeState(const QString& username, uint8_t newStatus);
    void requestUserInfo(const QString& username);
    void setUserInfoCallback(std::function<void(const QString&, int)> callback);

private slots:
    void sendMessage();         // Enviar mensaje en chat personal
    void sendGeneralMessage();  // Nuevo slot para enviar mensaje en chat general
    void receiveMessage(const QString& message);
    void onStateChanged(int index);
    void showChatMessages(const QString& user2);
    QString get_chat_id(const QString& user2);

private:
    QWebSocket& socket;
    // Componentes para el chat general
    QLineEdit* generalMessageInput;
    QPushButton* generalSendButton;
    QTextEdit* generalChatArea;
    
    // Componentes para el chat personal
    QLineEdit* messageInput;
    QPushButton* sendButton;
    QTextEdit* chatArea;
    QComboBox* userList;
    QComboBox* stateList;
    QLineEdit* usernameInput;
    QByteArray buildMessage(quint8 type, const QString& param1, const QString& param2 = "");
    
    // Callback para manejar información de usuario
    std::function<void(const QString&, int)> m_userInfoCallback;
};

#endif // MESSAGEHANDLER_H