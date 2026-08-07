#include "TcpServer.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>

// ---------------------------------------------------------------------------
// TcpServer implementation
// ---------------------------------------------------------------------------

TcpServer::TcpServer(int connectionId, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_connectionId(connectionId)
{
    connect(m_server, SIGNAL(newConnection()),
            this, SLOT(onNewConnection()));
}

TcpServer::~TcpServer()
{
    // stopListening() корректно закрывает все сессии и сервер
    stopListening();
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

bool TcpServer::isListening() const
{
    return m_server->isListening();
}

int TcpServer::connectionId() const
{
    return m_connectionId;
}

quint16 TcpServer::port() const
{
    return m_server->serverPort();
}

QHostAddress TcpServer::address() const
{
    return m_server->serverAddress();
}

int TcpServer::clientCount() const
{
    return m_sessions.size();
}

QList<ClientSession> TcpServer::sessions() const
{
    // Возвращаем копию — вызывающий код не может изменить внутреннее состояние
    return m_sessions.values();
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

bool TcpServer::startListening(const QHostAddress &address, quint16 port)
{
    if (m_server->isListening()) {
        qDebug() << "[TcpServer] id=" << m_connectionId
                 << "already listening on port" << m_server->serverPort();
        return true;
    }

    if (!m_server->listen(address, port)) {
        const QString errStr = m_server->errorString();
        qWarning() << "[TcpServer] id=" << m_connectionId
                   << "failed to start:" << errStr;
        emit errorOccurred(m_connectionId, errStr);
        return false;
    }

    qDebug() << "[TcpServer] id=" << m_connectionId
             << "listening on" << m_server->serverAddress().toString()
             << "port" << m_server->serverPort();

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::TcpServer,
        QString("Server started on %1:%2")
            .arg(m_server->serverAddress().toString())
            .arg(m_server->serverPort())
        );
    emit messageReceived(msg);
    emit listeningChanged(true);

    return true;
}

void TcpServer::stopListening()
{
    if (!m_server->isListening() && m_sessions.isEmpty()) {
        return;
    }

    // Закрываем все клиентские сокеты до остановки сервера
    // Копируем ключи — removeSession() модифицирует m_sessions во время итерации
    const QList<qintptr> descriptors = m_sessions.keys();
    for (qintptr descriptor : descriptors) {
        removeSession(descriptor);
    }

    m_server->close();

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::TcpServer,
        QString("Server stopped")
        );
    emit messageReceived(msg);
    emit listeningChanged(false);
}

bool TcpServer::sendToClient(qintptr descriptor, const QByteArray &data)
{
    if (!m_sessions.contains(descriptor)) {
        qWarning() << "[TcpServer] id=" << m_connectionId
                   << "sendToClient: unknown descriptor" << descriptor;
        return false;
    }

    if (data.isEmpty()) {
        return false;
    }

    QTcpSocket *socket = m_sessions[descriptor].socket;

    const qint64 written = socket->write(data);
    if (written == -1) {
        qWarning() << "[TcpServer] id=" << m_connectionId
                   << "write() failed for descriptor" << descriptor
                   << ":" << socket->errorString();
        return false;
    }

    const Message msg = Message::outgoing(
        m_connectionId,
        Message::Protocol::TcpServer,
        data,
        false
        );
    emit messageReceived(msg);

    return true;
}

void TcpServer::broadcast(const QByteArray &data)
{
    if (data.isEmpty() || m_sessions.isEmpty()) {
        return;
    }

    for (const ClientSession &session : m_sessions) {
        session.socket->write(data);
    }

    // Одно сообщение в лог для всей рассылки — не по одному на клиента
    const Message msg = Message::outgoing(
        m_connectionId,
        Message::Protocol::TcpServer,
        data,
        false
        );
    emit messageReceived(msg);
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void TcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();

        // nextPendingConnection() возвращает сокет без parent —
        // берём владение явно чтобы не было утечки
        socket->setParent(this);

        ClientSession session;
        session.descriptor  = socket->socketDescriptor();
        session.address     = socket->peerAddress().toString();
        session.port        = socket->peerPort();
        session.socket      = socket;

        m_sessions.insert(session.descriptor, session);

        connect(socket, SIGNAL(readyRead()),
                this, SLOT(onClientReadyRead()));

        connect(socket, SIGNAL(disconnected()),
                this, SLOT(onClientDisconnected()));

        qDebug() << "[TcpServer] id=" << m_connectionId
                 << "new client:" << session.displayName()
                 << "descriptor:" << session.descriptor;

        const Message msg = Message::system(
            m_connectionId,
            Message::Protocol::TcpServer,
            QString("Client connected: %1").arg(session.displayName())
            );
        emit messageReceived(msg);
        emit clientConnected(session.descriptor, session.displayName());
    }
}

void TcpServer::onClientReadyRead()
{
    ClientSession *session = sessionBySender();
    if (!session) {
        return;
    }

    const QByteArray data = session->socket->readAll();
    if (data.isEmpty()) {
        return;
    }

    const Message msg = Message::incoming(
        m_connectionId,
        Message::Protocol::TcpServer,
        data,
        false
        );
    emit messageReceived(msg);
}

void TcpServer::onClientDisconnected()
{
    ClientSession *session = sessionBySender();
    if (!session) {
        return;
    }

    const qintptr   descriptor  = session->descriptor;
    const QString   displayName = session->displayName();

    qDebug() << "[TcpServer] id=" << m_connectionId
             << "client disconnected:" << displayName;

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::TcpServer,
        QString("Client disconnected: %1").arg(displayName)
        );
    emit messageReceived(msg);
    emit clientDisconnected(descriptor, displayName);

    // Удаляем после emit — сигнал может использовать данные сессии
    removeSession(descriptor);
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

ClientSession *TcpServer::sessionBySender()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        qWarning() << "[TcpServer] sessionBySender: sender is not QTcpSocket";
        return nullptr;
    }

    const qintptr descriptor = socket->socketDescriptor();

    // socketDescriptor() возвращает -1 если сокет уже закрыт
    // В этом случае ищем по указателю
    if (descriptor != -1 && m_sessions.contains(descriptor)) {
        return &m_sessions[descriptor];
    }

    // Fallback: линейный поиск по указателю на сокет
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().socket == socket) {
            return &it.value();
        }
    }

    qWarning() << "[TcpServer] sessionBySender: session not found for socket";
    return nullptr;
}

void TcpServer::removeSession(qintptr descriptor)
{
    if (!m_sessions.contains(descriptor)) {
        return;
    }

    QTcpSocket *socket = m_sessions[descriptor].socket;
    m_sessions.remove(descriptor);

    // abort() — мгновенное закрытие без ожидания flush
    // disconnected() сигнал не придёт после abort() если сокет уже закрыт
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    // deleteLater() безопаснее delete: сокет может быть в стеке вызовов
    socket->deleteLater();
}