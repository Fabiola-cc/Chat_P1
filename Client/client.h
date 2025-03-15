#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QWidget>
#include <QWebSocket>

class ChatClient : public QWidget {
    Q_OBJECT  // ← Necesario para señales y ranuras

public:
    explicit ChatClient(QWidget *parent = nullptr);

private:
    QWebSocket socket;
};

#endif // ChatClient_H
