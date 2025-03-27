#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include <QWebSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>

class MessageHandler : public QObject {
    Q_OBJECT

public:
    explicit MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, 
                            QTextEdit* chatArea, QComboBox* userList, QComboBox* stateList, QLineEdit* usernameInput, QObject* parent = nullptr);

    void requestChatHistory(const QString& chatName);
    void requestChangeState(const QString& username, uint8_t newStatus);

private slots:
    void sendMessage();
    void receiveMessage(const QString& message);
    void onStateChanged(int index); 

private:
    QWebSocket& socket;
    QLineEdit* messageInput;
    QPushButton* sendButton;
    QTextEdit* chatArea;
    QComboBox* userList;
    QComboBox* stateList;
    QLineEdit* usernameInput;  
    QByteArray buildMessage(quint8 type, const QString& param1, const QString& param2 = "");
};


#endif // MESSAGE_HANDLER_H
