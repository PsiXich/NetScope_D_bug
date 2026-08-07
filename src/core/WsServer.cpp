#include "WsServer.h"

#include <QWebSocket>
#include <QDebug>

WsServer::WsServer(int connectionId, QObject *parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(
          QStringLiteral("NetScopeWsServer"),
          QWebSocketServer::NonSecureMode,
          this))
    , m_connectionId(connectionId)
{
    connect(m_server, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);

    connect(m_server, &QWebSocketServer::serverError,
            this, &WsServer::onServerError);
}

WsServer::~WsServer()
{
    stopListening();
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

bool WsServer::isListening() const
{
    return m_server->isListening();
}

int WsServer::connectionId() const
{
    return m_connectionId;
}

quint16 WsServer::port() const
{
    return m_server->serverPort();
}

QHostAddress WsServer::address() const
{
    return m_server->serverAddress();
}

int WsServer::clientCount() const
{
    return m_sessions.size();
}

QList<WsClientSession> WsServer::sessions() const
{
    return m_sessions.values();
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

bool WsServer::startListening(const QHostAddress &address,
                              quint16 port,
                              const QString &serverName)
{
    Q_UNUSED(serverName)

    if (m_server->isListening()) {
        qDebug() << "[WsServer] id=" << m_connectionId
                 << "already listening on port" << m_server->serverPort();
        return true;
    }

    if (!m_server->listen(address, port)) {
        const QString errStr = m_server->errorString();
        qWarning() << "[WsServer] id=" << m_connectionId
                   << "failed to start:" << errStr;
        emit errorOccurred(m_connectionId, errStr);
        return false;
    }

    qDebug() << "[WsServer] id=" << m_connectionId
             << "listening on" << m_server->serverAddress().toString()
             << "port" << m_server->serverPort();

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::WsServer,
        QString("WebSocket server started on ws://%1:%2")
            .arg(m_server->serverAddress().toString())
            .arg(m_server->serverPort())
        ));
    emit listeningChanged(true);

    return true;
}

void WsServer::stopListening()
{
    if (!m_server->isListening() && m_sessions.isEmpty()) {
        return;
    }

    const QList<int> ids = m_sessions.keys();
    for (int sessionId : ids) {
        removeSession(sessionId);
    }

    m_server->close();

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::WsServer,
        QStringLiteral("WebSocket server stopped")
        ));
    emit listeningChanged(false);
}

bool WsServer::sendTextToClient(int sessionId, const QString &text)
{
    if (!m_sessions.contains(sessionId) || text.isEmpty()) {
        return false;
    }

    QWebSocket *socket = m_sessions[sessionId].socket;

    if (socket->sendTextMessage(text) == -1) {
        return false;
    }

    emit messageReceived(Message::outgoing(
        m_connectionId, Message::Protocol::WsServer, text.toUtf8(), true
        ));
    return true;
}

bool WsServer::sendBinaryToClient(int sessionId, const QByteArray &data)
{
    if (!m_sessions.contains(sessionId) || data.isEmpty()) {
        return false;
    }

    QWebSocket *socket = m_sessions[sessionId].socket;

    if (socket->sendBinaryMessage(data) == -1) {
        return false;
    }

    emit messageReceived(Message::outgoing(
        m_connectionId, Message::Protocol::WsServer, data, false
        ));
    return true;
}

void WsServer::broadcastText(const QString &text)
{
    if (text.isEmpty() || m_sessions.isEmpty()) {
        return;
    }
    for (const WsClientSession &s : m_sessions) {
        s.socket->sendTextMessage(text);
    }
    emit messageReceived(Message::outgoing(
        m_connectionId, Message::Protocol::WsServer, text.toUtf8(), true
        ));
}

void WsServer::broadcastBinary(const QByteArray &data)
{
    if (data.isEmpty() || m_sessions.isEmpty()) {
        return;
    }
    for (const WsClientSession &s : m_sessions) {
        s.socket->sendBinaryMessage(data);
    }
    emit messageReceived(Message::outgoing(
        m_connectionId, Message::Protocol::WsServer, data, false
        ));
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void WsServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket *socket = m_server->nextPendingConnection();
        socket->setParent(this);

        WsClientSession session;
        session.id      = m_nextSessionId++;
        session.address = socket->peerAddress().toString();
        session.port    = socket->peerPort();
        session.socket  = socket;

        m_sessions.insert(session.id, session);

        connect(socket, &QWebSocket::textMessageReceived,
                this, &WsServer::onTextMessageReceived);

        connect(socket, &QWebSocket::binaryMessageReceived,
                this, &WsServer::onBinaryMessageReceived);

        connect(socket, &QWebSocket::disconnected,
                this, &WsServer::onClientDisconnected);

        qDebug() << "[WsServer] id=" << m_connectionId
                 << "new client:" << session.displayName()
                 << "sessionId:" << session.id;

        emit messageReceived(Message::system(
            m_connectionId,
            Message::Protocol::WsServer,
            QString("Client connected: %1").arg(session.displayName())
            ));
        emit clientConnected(session.id, session.displayName());
    }
}

void WsServer::onTextMessageReceived(const QString &message)
{
    if (!sessionBySender()) {
        return;
    }
    emit messageReceived(Message::incoming(
        m_connectionId, Message::Protocol::WsServer, message.toUtf8(), true
        ));
}

void WsServer::onBinaryMessageReceived(const QByteArray &message)
{
    if (!sessionBySender()) {
        return;
    }
    emit messageReceived(Message::incoming(
        m_connectionId, Message::Protocol::WsServer, message, false
        ));
}

void WsServer::onClientDisconnected()
{
    WsClientSession *session = sessionBySender();
    if (!session) {
        return;
    }

    const int     sessionId   = session->id;
    const QString displayName = session->displayName();

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::WsServer,
        QString("Client disconnected: %1").arg(displayName)
        ));
    emit clientDisconnected(sessionId, displayName);

    removeSession(sessionId);
}

void WsServer::onServerError(QWebSocketProtocol::CloseCode closeCode)
{
    Q_UNUSED(closeCode)
    const QString errStr = m_server->errorString();
    qWarning() << "[WsServer] id=" << m_connectionId << "error:" << errStr;

    emit messageReceived(Message::system(
        m_connectionId, Message::Protocol::WsServer,
        QString("Server error: %1").arg(errStr)
        ));
    emit errorOccurred(m_connectionId, errStr);
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

WsClientSession *WsServer::sessionBySender()
{
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket) {
        return nullptr;
    }

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().socket == socket) {
            return &it.value();
        }
    }
    return nullptr;
}

void WsServer::removeSession(int sessionId)
{
    if (!m_sessions.contains(sessionId)) {
        return;
    }

    QWebSocket *socket = m_sessions[sessionId].socket;
    m_sessions.remove(sessionId);

    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->close();
    }
    socket->deleteLater();
}