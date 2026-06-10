#ifndef NETSCOPE_WSSERVER_H
#define NETSCOPE_WSSERVER_H

#include "Message.h"

#include <QObject>
#include <QWebSocketServer>
#include <QHostAddress>
#include <QMap>
#include <QUrl>

class QWebSocket;

// ---------------------------------------------------------------------------
// WsClientSession — описывает одно входящее WebSocket-соединение
// ---------------------------------------------------------------------------
struct WsClientSession
{
    int          id       { -1 };       // наш внутренний id сессии
    QString      address;               // удалённый IP
    quint16      port     { 0 };        // удалённый порт
    QWebSocket  *socket   { nullptr };

    QString displayName() const
    {
        return QString("%1:%2").arg(address).arg(port);
    }
};

// ---------------------------------------------------------------------------
// WsServer — принимает входящие WebSocket-соединения
//
// Ответственность:
//   - слушать порт (NonSecureMode — без TLS)
//   - принимать WebSocket handshake (Qt делает автоматически)
//   - рассылать text / binary фреймы одному или всем клиентам
//   - уведомлять подписчиков о сессиях и входящих данных
//
// Отличия от TcpServer:
//   - QWebSocketServer вместо QTcpServer
//   - каждый клиент QWebSocket* а не QTcpSocket*
//   - два метода отправки: text frame и binary frame
//   - сессии идентифицируются по int id (не qintptr дескриптор)
//     потому что QWebSocket::socketDescriptor() ненадёжен на Windows
//
// NonSecureMode — достаточно для отладки на localhost
// wss:// (TLS) todo позже через QSslConfiguration
// ---------------------------------------------------------------------------
class WsServer : public QObject
{
    Q_OBJECT

public:
    explicit WsServer(int connectionId, QObject *parent = nullptr);
    ~WsServer() override;

    // --- Запросы состояния ---
    bool         isListening()   const;
    int          connectionId()  const;
    quint16      port()          const;
    QHostAddress address()       const;
    int          clientCount()   const;

    // Список активных сессий
    QList<WsClientSession> sessions() const;

public slots:
    // Начать прослушивание
    // serverName используется в HTTP Upgrade handshake (поле Server:)
    // port = 0 — ОС выбирает свободный порт
    bool startListening(const QHostAddress &address = QHostAddress::Any,
                        quint16 port = 0,
                        const QString &serverName = "NetScopeWsServer");

    // Остановить сервер и закрыть все активные сессии
    void stopListening();

    // Отправить text frame конкретному клиенту по его session id
    bool sendTextToClient(int sessionId, const QString &text);

    // Отправить binary frame конкретному клиенту
    bool sendBinaryToClient(int sessionId, const QByteArray &data);

    // Разослать text frame всем подключённым клиентам
    void broadcastText(const QString &text);

    // Разослать binary frame всем подключённым клиентам
    void broadcastBinary(const QByteArray &data);

signals:
    // Новый клиент подключился
    void clientConnected(int sessionId, const QString &displayName);

    // Клиент отключился
    void clientDisconnected(int sessionId, const QString &displayName);

    // Получено сообщение от клиента
    // isText определяет тип фрейма (text / binary)
    void messageReceived(const Message &message);

    // Ошибка сервера
    void errorOccurred(int connectionId, const QString &errorString);

    // Сервер начал / прекратил слушать
    void listeningChanged(bool listening);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &message);
    void onClientDisconnected();
    void onServerError(QWebSocketProtocol::CloseCode closeCode);

private:
    // Найти сессию по QWebSocket* из sender()
    WsClientSession *sessionBySender();

    // Закрыть и удалить сессию
    void removeSession(int sessionId);

    QWebSocketServer            *m_server   { nullptr };
    QMap<int, WsClientSession>   m_sessions; // sessionId → session

    int  m_connectionId;
    int  m_nextSessionId { 0 };  // инкрементный id для сессий
};

#endif // NETSCOPE_WSSERVER_H