#include "TcpClient.h"

#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

// ---------------------------------------------------------------------------
// TcpClient implementation
// ---------------------------------------------------------------------------

TcpClient::TcpClient(int connectionId, QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_connectionId(connectionId)
{
    // --- Настройка таймера переподключения ---
    // singleShot = false: таймер повторяется пока не подключимся.
    // Останавливается в onConnected() и teardownReconnect()
    m_reconnectTimer->setSingleShot(false);

    // --- Подключение сигналов сокета ---
    // Используем старый синтаксис SIGNAL/SLOT для Qt 5.7 совместимости
    // с перегруженным сигналом error(QAbstractSocket::SocketError)
    connect(m_socket, SIGNAL(connected()),
            this, SLOT(onConnected()));

    connect(m_socket, SIGNAL(disconnected()),
            this, SLOT(onDisconnected()));

    connect(m_socket, SIGNAL(readyRead()),
            this, SLOT(onReadyRead()));

    // error() перегружен в Qt5 — строковый синтаксис обходит эту проблему
    // В Qt6 этот сигнал переименован в errorOccurred() — при миграции
    // достаточно поменять строку здесь
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));

    connect(m_reconnectTimer, SIGNAL(timeout()),
            this, SLOT(onReconnectTimer()));
}

TcpClient::~TcpClient()
{
    // QTcpSocket и QTimer удалятся автоматически (parent = this)
    // Явно закрываем сокет чтобы не получить сигнал disconnected()
    // во время уничтожения объекта — слоты могут быть уже недоступны
    teardownReconnect();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();  // abort() — немедленно, без ожидания flush
    }
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

TcpClient::State TcpClient::state() const
{
    return m_state;
}

int TcpClient::connectionId() const
{
    return m_connectionId;
}

QString TcpClient::host() const
{
    return m_host;
}

quint16 TcpClient::port() const
{
    return m_port;
}

int TcpClient::reconnectInterval() const
{
    return m_reconnectInterval;
}

void TcpClient::setReconnectInterval(int ms)
{
    m_reconnectInterval = (ms > 0) ? ms : 0;
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void TcpClient::connectToHost(const QString &host, quint16 port)
{
    if (m_state != State::Disconnected) {
        qDebug() << "[TcpClient] id=" << m_connectionId
                 << "already connecting or connected, ignoring.";
        return;
    }

    m_host = host;
    m_port = port;
    m_intentionalDisconnect = false;

    setState(State::Connecting);
    m_socket->connectToHost(host, port);
}

void TcpClient::disconnectFromHost()
{
    // Помечаем как намеренный разрыв — onDisconnected() не запустит реконнект
    m_intentionalDisconnect = true;
    teardownReconnect();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        // disconnectFromHost() — мягкое закрытие: ждёт flush буфера
        // Если сервер не отвечает — сокет закроется по внутреннему таймауту
        m_socket->disconnectFromHost();
    }
}

bool TcpClient::sendData(const QByteArray &data)
{
    if (m_state != State::Connected) {
        qWarning() << "[TcpClient] id=" << m_connectionId
                   << "sendData() called while not connected.";
        return false;
    }

    if (data.isEmpty()) {
        return false;
    }

    const qint64 written = m_socket->write(data);

    if (written == -1) {
        qWarning() << "[TcpClient] id=" << m_connectionId
                   << "write() failed:" << m_socket->errorString();
        return false;
    }

    // Публикуем исходящее сообщение в лог
    const Message msg = Message::outgoing(
        m_connectionId,
        Message::Protocol::Tcp,
        data,
        false   // TCP — бинарный поток, isText определяет получатель
        );
    emit messageReceived(msg);

    return true;
}

// ---------------------------------------------------------------------------
// Приватные слоты — обработчики событий сокета
// ---------------------------------------------------------------------------

void TcpClient::onConnected()
{
    teardownReconnect();    // останавливаем таймер реконнекта если был запущен
    setState(State::Connected);

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::Tcp,
        QString("Connected to %1:%2").arg(m_host).arg(m_port)
        );
    emit messageReceived(msg);
    emit connected(m_connectionId);
}

void TcpClient::onDisconnected()
{
    setState(State::Disconnected);

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::Tcp,
        QString("Disconnected from %1:%2").arg(m_host).arg(m_port)
        );
    emit messageReceived(msg);
    emit disconnected(m_connectionId);

    // Запускаем автореконнект только если разрыв был не намеренным
    if (!m_intentionalDisconnect && m_reconnectInterval > 0) {
        setupReconnect();
    }
}

void TcpClient::onReadyRead()
{
    // Читаем всё доступное за один вызов
    // readAll() безопасен — QTcpSocket буферизует данные
    // Для больших объёмов в будущем можно заменить на цикл с read(chunkSize)
    const QByteArray data = m_socket->readAll();

    if (data.isEmpty()) {
        return;
    }

    const Message msg = Message::incoming(
        m_connectionId,
        Message::Protocol::Tcp,
        data,
        false   // isText = false: TCP не имеет понятия text/binary frame
        );
    emit messageReceived(msg);
}

void TcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    // QAbstractSocket::RemoteHostClosedError — нормальное закрытие соединения
    // со стороны сервера Не логируем как ошибку, onDisconnected() справится
    if (error == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    const QString errStr = m_socket->errorString();

    qWarning() << "[TcpClient] id=" << m_connectionId
               << "socket error:" << errStr;

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::Tcp,
        QString("Error: %1").arg(errStr)
        );
    emit messageReceived(msg);
    emit errorOccurred(m_connectionId, errStr);
}

void TcpClient::onReconnectTimer()
{
    if (m_state != State::Disconnected) {
        return;
    }

    qDebug() << "[TcpClient] id=" << m_connectionId
             << "attempting reconnect to" << m_host << m_port;

    setState(State::Connecting);
    m_socket->connectToHost(m_host, m_port);
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

void TcpClient::setState(State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    emit stateChanged(m_connectionId, m_state);
}

void TcpClient::setupReconnect()
{
    if (m_reconnectInterval <= 0) {
        return;
    }

    const Message msg = Message::system(
        m_connectionId,
        Message::Protocol::Tcp,
        QString("Reconnecting in %1 ms...").arg(m_reconnectInterval)
        );
    emit messageReceived(msg);

    m_reconnectTimer->start(m_reconnectInterval);
}

void TcpClient::teardownReconnect()
{
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}