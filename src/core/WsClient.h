#ifndef NETSCOPE_WSCLIENT_H
#define NETSCOPE_WSCLIENT_H

#include "Message.h"

#include <QObject>
#include <QAbstractSocket>
#include <QUrl>
#include <QWebSocketProtocol>

class QWebSocket;
class QTimer;

// ---------------------------------------------------------------------------
// WsClient — управляет одним WebSocket-соединением (RFC 6455)
//
// WebSocket принципиально отличается от TCP двумя вещами:
//   1. Фреймы — данные делятся на text frame и binary frame.
//      Text frame гарантированно UTF-8, binary — произвольные байты.
//      isText в Message выставляется соответственно типу фрейма
//
//   2. Ping/Pong — протокол имеет встроенный heartbeat
//      QWebSocket отвечает на Ping автоматически, но мы также
//      отправляем Ping сами через QTimer чтобы детектировать
//      "мёртвые" соединения (сервер не отвечает на Ping → disconnect)
//
// Интерфейс намеренно повторяет TcpClient где возможно —
// UI-панели могут использовать похожую логику подключения
// ---------------------------------------------------------------------------
class WsClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected
    };
    Q_ENUM(State)

    explicit WsClient(int connectionId, QObject *parent = nullptr);
    ~WsClient() override;

    // --- Запросы состояния ---
    State       state()             const;
    int         connectionId()      const;
    QUrl        url()               const;

    // Интервал ping-фреймов в мс. 0 = ping выключен
    // Рекомендуемое значение: 15000–30000 мс.
    int         pingInterval()      const;
    void        setPingInterval(int ms);

    // Интервал автореконнекта в мс. 0 = выключен
    int         reconnectInterval() const;
    void        setReconnectInterval(int ms);

public slots:
    // Открыть соединение по URL вида "ws://host:port/path"
    // или "wss://host:port/path" для TLS (если Qt собран с SSL)
    void connectToUrl(const QUrl &url);

    // Корректное закрытие с отправкой Close frame
    void disconnectFromHost();

    // Отправить текстовый фрейм (UTF-8)
    // Используется для JSON, plain text протоколов
    bool sendTextMessage(const QString &message);

    // Отправить бинарный фрейм
    // Используется для произвольных байтовых данных
    bool sendBinaryMessage(const QByteArray &data);

signals:
    void connected(int connectionId);
    void disconnected(int connectionId);

    // Входящий text или binary фрейм — тип определяется msg.isText
    void messageReceived(const Message &message);

    // Ошибка сокета
    void errorOccurred(int connectionId, const QString &errorString);

    // Состояние изменилось
    void stateChanged(int connectionId, WsClient::State state);

    // Pong получен от сервера — полезно для измерения latency в будущем
    void pongReceived(int connectionId, quint64 elapsedMs, const QByteArray &payload);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &message);
    void onError(QAbstractSocket::SocketError error);
    void onPong(quint64 elapsedMs, const QByteArray &payload);
    void onPingTimer();
    void onReconnectTimer();

private:
    void setState(State state);
    void setupPing();
    void teardownPing();
    void setupReconnect();
    void teardownReconnect();

    QWebSocket  *m_socket           { nullptr };
    QTimer      *m_pingTimer        { nullptr };
    QTimer      *m_reconnectTimer   { nullptr };

    int          m_connectionId;
    QUrl         m_url;
    State        m_state            { State::Disconnected };

    int          m_pingInterval     { 0 };
    int          m_reconnectInterval{ 0 };

    bool         m_intentionalDisconnect { false };

    // Время последней отправки Ping — для вычисления RTT в pongReceived()
    quint64      m_lastPingSentMs   { 0 };
};

#endif // NETSCOPE_WSCLIENT_H
