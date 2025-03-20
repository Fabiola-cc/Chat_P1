#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <QObject>
#include <QWebSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QByteArray>

class MessageHandler : public QObject {
    Q_OBJECT

public:
    explicit MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, QTextEdit* chatArea, QComboBox* userList, QObject* parent = nullptr);

public slots:
    void sendMessage();
    void receiveMessage(const QString& message);
    void requestChatHistory(const QString& chatName);

private:
    QWebSocket& socket;
    QLineEdit* messageInput;
    QPushButton* sendButton;
    QTextEdit* chatArea;
    QComboBox* userList;

    QByteArray buildMessage(quint8 type, const QString& param1 = "", const QString& param2 = "");
};

#endif // MESSAGE_HANDLER_H
