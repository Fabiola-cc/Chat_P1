#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QWidget>
#include <QWebSocket>

class WebSocketClient : public QWidget {
    Q_OBJECT  // ← Necesario para señales y ranuras

public:
    explicit WebSocketClient(QWidget *parent = nullptr);

private:
    QWebSocket socket;
};

#endif // WEBSOCKETCLIENT_H
