#include "WsClient.h"

#include <QWebSocket>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

// ---------------------------------------------------------------------------
// WsClient implementation
// ---------------------------------------------------------------------------

WsClient::WsClient(int connectionId, QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_pingTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_connectionId(connectionId)
{
    // Ping таймер — повторяется пока соединение живо
    m_pingTimer->setSingleShot(false);
    m_reconnectTimer->setSingleShot(false);

    // --- Сигналы QWebSocket ---
    connect(m_socket, SIGNAL(connected()),
            this, SLOT(onConnected()));

    connect(m_socket, SIGNAL(disconnected()),
            this, SLOT(onDisconnected()));

    connect(m_socket, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));

    connect(m_socket, SIGNAL(binaryMessageReceived(QByteArray)),
            this, SLOT(onBinaryMessageReceived(QByteArray)));

    // error() перегружен — используем старый синтаксис как в TcpClient
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onError(QAbstractSocket::SocketError)));

    connect(m_socket, SIGNAL(pong(quint64, QByteArray)),
            this, SLOT(onPong(quint64, QByteArray)));

    connect(m_pingTimer, SIGNAL(timeout()),
            this, SLOT(onPingTimer()));

    connect(m_reconnectTimer, SIGNAL(timeout()),
            this, SLOT(onReconnectTimer()));
}

WsClient::~WsClient()
{
    teardownPing();
    teardownReconnect();

    // abort() — немедленное закрытие без отправки Close frame
    // В деструкторе нет смысла ждать handshake закрытия
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

WsClient::State WsClient::state() const
{
    return m_state;
}

int WsClient::connectionId() const
{
    return m_connectionId;
}

QUrl WsClient::url() const
{
    return m_url;
}

int WsClient::pingInterval() const
{
    return m_pingInterval;
}

void WsClient::setPingInterval(int ms)
{
    m_pingInterval = (ms > 0) ? ms : 0;

    // Если уже подключены — перезапускаем ping с новым интервалом
    if (m_state == State::Connected) {
        teardownPing();
        setupPing();
    }
}

int WsClient::reconnectInterval() const
{
    return m_reconnectInterval;
}

void WsClient::setReconnectInterval(int ms)
{
    m_reconnectInterval = (ms > 0) ? ms : 0;
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void WsClient::connectToUrl(const QUrl &url)
{
    if (m_state != State::Disconnected) {
        qDebug() << "[WsClient] id=" << m_connectionId
                 << "already connecting or connected, ignoring.";
        return;
    }

    if (!url.isValid()) {
        const QString errStr = QString("Invalid URL: %1").arg(url.toString());
        qWarning() << "[WsClient] id=" << m_connectionId << errStr;
        emit errorOccurred(m_connectionId, errStr);
        return;
    }

    m_url = url;
    m_intentionalDisconnect = false;

    setState(State::Connecting);

    // QWebSocket::open() начинает HTTP Upgrade handshake асинхронно
    // connected() придёт когда handshake завершён успешно
    m_socket->open(url);
}

void WsClient::disconnectFromHost()
{
    m_intentionalDisconnect = true;
    teardownPing();
    teardownReconnect();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        // close() отправляет WebSocket Close frame с кодом Normal Closure (1000)
        // и ждёт ответного Close frame от сервера — корректное завершение по RFC 6455
        m_socket->close();
    }
}

bool WsClient::sendTextMessage(const QString &message)
{
    if (m_state != State::Connected) {
        qWarning() << "[WsClient] id=" << m_connectionId
                   << "sendTextMessage() called while not connected.";
        return false;
    }

    if (message.isEmpty()) {
        return false;
    }

    const qint64 written = m_socket->sendTextMessage(message);

    if (written == -1) {
        qWarning() << "[WsClient] id=" << m_connectionId
                   << "sendTextMessage() failed:" << m_socket->errorString();
        return false;
    }

    // Сохраняем как UTF-8 байты в payload — единый формат для всего лога
    const Message msg = Message::outgoing(
        m_connectionId,
        Message::Protocol::WebSocket,
        message.toUtf8(),
        true    // isText = true: это text frame
        );
    emit messageReceived(msg);

    return true;
}

bool WsClient::sendBinaryMessage(const QByteArray &data)
{
    if (m_state != State::Connected) {
        qWarning() << "[WsClient] id=" << m_connectionId
                   << "sendBinaryMessage() called while not connected.";
        return false;
    }

    if (data.isEmpty()) {
        return false;
    }

    const qint64 written = m_socket->sendBinaryMessage(data);

    if (written == -1) {
        qWarning() << "[WsClient] id=" << m_connectionId
                   << "sendBinaryMessage() failed:" << m_socket->errorString();
        return false;
    }

    const Message msg = Message::outgoing(
        m_connectionId,
        Message::Protocol::WebSocket,
        data,
        false   // isText = false: это binary frame
        );
    emit messageReceived(msg);

    return true;
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void WsClient::onConnected()
{
    teardownReconnect();
    setState(State::Connected);
    setupPing();

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::WebSocket,
        QString("Connected to %1").arg(m_url.toString())
        );
    emit messageReceived(msg);
    emit connected(m_connectionId);
}

void WsClient::onDisconnected()
{
    teardownPing();
    setState(State::Disconnected);

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::WebSocket,
        QString("Disconnected from %1").arg(m_url.toString())
        );
    emit messageReceived(msg);
    emit disconnected(m_connectionId);

    if (!m_intentionalDisconnect && m_reconnectInterval > 0) {
        setupReconnect();
    }
}

void WsClient::onTextMessageReceived(const QString &message)
{
    const Message msg = Message::incoming(
        m_connectionId,
        Message::Protocol::WebSocket,
        message.toUtf8(),
        true    // isText = true: WebSocket text frame гарантированно UTF-8
        );
    emit messageReceived(msg);
}

void WsClient::onBinaryMessageReceived(const QByteArray &message)
{
    const Message msg = Message::incoming(
        m_connectionId,
        Message::Protocol::WebSocket,
        message,
        false   // isText = false: binary frame
        );
    emit messageReceived(msg);
}

void WsClient::onError(QAbstractSocket::SocketError error)
{
    // HandshakeFailedError может прийти вместе с disconnected() —
    // не фильтруем в отличие от TcpClient::RemoteHostClosedError
    Q_UNUSED(error)

    const QString errStr = m_socket->errorString();

    qWarning() << "[WsClient] id=" << m_connectionId
               << "socket error:" << errStr;

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::WebSocket,
        QString("Error: %1").arg(errStr)
        );
    emit messageReceived(msg);
    emit errorOccurred(m_connectionId, errStr);
}

void WsClient::onPong(quint64 elapsedMs, const QByteArray &payload)
{
    qDebug() << "[WsClient] id=" << m_connectionId
             << "pong received, RTT:" << elapsedMs << "ms";

    emit pongReceived(m_connectionId, elapsedMs, payload);
}

void WsClient::onPingTimer()
{
    if (m_state != State::Connected) {
        return;
    }

    // Сохраняем время отправки для вычисления RTT в onPong()
    // QWebSocket::ping() сам измеряет время и передаёт elapsedMs в pong(),
    // но мы дублируем на случай если захотим считать RTT иначе
    m_lastPingSentMs = static_cast<quint64>(
        QDateTime::currentMSecsSinceEpoch()
        );

    // payload = пустой — нам не нужна дополнительная идентификация ping
    m_socket->ping();
}

void WsClient::onReconnectTimer()
{
    if (m_state != State::Disconnected) {
        return;
    }

    qDebug() << "[WsClient] id=" << m_connectionId
             << "attempting reconnect to" << m_url.toString();

    setState(State::Connecting);
    m_socket->open(m_url);
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

void WsClient::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_connectionId, m_state);
}

void WsClient::setupPing()
{
    if (m_pingInterval <= 0) {
        return;
    }
    m_pingTimer->start(m_pingInterval);
}

void WsClient::teardownPing()
{
    if (m_pingTimer->isActive()) {
        m_pingTimer->stop();
    }
}

void WsClient::setupReconnect()
{
    if (m_reconnectInterval <= 0) {
        return;
    }

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::WebSocket,
        QString("Reconnecting in %1 ms...").arg(m_reconnectInterval)
        );
    emit messageReceived(msg);

    m_reconnectTimer->start(m_reconnectInterval);
}

void WsClient::teardownReconnect()
{
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}