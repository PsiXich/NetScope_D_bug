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
    connect(m_socket, &QTcpSocket::connected,      this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,   this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,      this, &TcpClient::onReadyRead);

// error() перегружен в Qt5 — строковый синтаксис обходит эту проблему
// В Qt6 этот сигнал переименован в errorOccurred() — при миграции
// достаточно поменять строку здесь
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &TcpClient::onSocketError);
#else
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif

    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::onReconnectTimer);
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
    m_intentionalDisconnect = true;
    teardownReconnect();

    QAbstractSocket::SocketState currentState = m_socket->state();

    if (currentState == QAbstractSocket::UnconnectedState) {
        // Уже отключены
        setState(State::Disconnected);
        emit disconnected(m_connectionId);
        return;
    }

    if (currentState == QAbstractSocket::ConnectingState) {
        m_socket->abort();  // прерываем попытку подключения
    } else {
        m_socket->disconnectFromHost();  // нормальное отключение
    }

    // Для тестов и надёжности — сразу обновляем состояние
    // (реальный сигнал disconnected() может прийти чуть позже)
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        setState(State::Disconnected);
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

    if (!m_intentionalDisconnect && m_reconnectInterval > 0) {
        setupReconnect();
    }

    m_intentionalDisconnect = false;   // сбрасываем флаг
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
    // RemoteHostClosedError — нормальное закрытие, обрабатывается в onDisconnected()
    if (error == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    const QString errStr = m_socket->errorString();

    qWarning() << "[TcpClient] id=" << m_connectionId
               << "socket error:" << errStr;

    // Важно: при ошибке переводим в Disconnected
    setState(State::Disconnected);

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