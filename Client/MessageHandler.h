#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QLabel>
#include <QTimer>
#include <QWebSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <functional> // Para usar std::function
#include <queue>
#include <unordered_map>


class MessageHandler : public QObject {
    Q_OBJECT

public:
    explicit MessageHandler(QWebSocket& socket, 
        QLineEdit* generalInput, QPushButton* generalButton, QTextEdit* generalChatArea,
        QLineEdit* input, QPushButton* button, QTextEdit* chatArea, 
        QComboBox* userList, QComboBox* stateList, QLineEdit* usernameInput, QLabel* notificationLabel, QTimer* notificationTimer,
        QObject* parent = nullptr);

    void requestChatHistory(const QString& chatName);
    void requestChangeState(const QString& username, uint8_t newStatus);
    void requestUserInfo(const QString& username);
    void setUserInfoCallback(std::function<void(const QString&, int)> callback);
    void setActualUser(const QString& username);
    const std::unordered_map<std::string, std::string>& getUserStates() const { 
        return userStates; 
    }
    void requestUsersList();
    void setUserListReceivedCallback(std::function<void(const std::unordered_map<std::string, std::string>&)> callback);


private slots:
    void sendMessage();         // Enviar mensaje en chat personal
    void sendGeneralMessage();  // Nuevo slot para enviar mensaje en chat general
    void receiveMessage(const QString& message);
    void onStateChanged(int index);
    void showChatMessages(const QString& user2);
    QString get_chat_id(const QString& user2);
    void storeMessage(const QString& sender, const QString& message);

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
    QLabel* notificationLabel;
    QTimer* notificationTimer;
    std::unordered_map<std::string, std::string> userStates;
    // Creador de mensajes para el server
    QByteArray buildMessage(quint8 type, const QString& param1, const QString& param2 = "");
    
    // Callback para manejar información de usuario
    std::function<void(const QString&, int)> m_userInfoCallback;

    //Otras variables
    QString actualUser;
    std::queue<QString> pendingHistoryRequests;
    std::function<void(const std::unordered_map<std::string, std::string>&)> m_userListReceivedCallback;
};

#endif // MESSAGEHANDLER_H