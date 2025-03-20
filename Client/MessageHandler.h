#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <QObject>
#include <QWebSocket>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QByteArray>

class MessageHandler : public QObject {
    Q_OBJECT

public:
    explicit MessageHandler(QWebSocket& socket, QLineEdit* input, QPushButton* button, QTextEdit* chatArea, QObject* parent = nullptr);

public slots:
    void sendMessage();
    void receiveMessage(const QString& message);
    void requestChatHistory(const QString& chatName);  // 🔹 Nueva función

private:
    QWebSocket& socket;
    QLineEdit* messageInput;
    QPushButton* sendButton;
    QTextEdit* chatArea;

    QByteArray buildMessage(quint8 type, const QString& param1 = "", const QString& param2 = "");
};

#endif // MESSAGE_HANDLER_H
