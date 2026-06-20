#include "UdpEndpoint.h"

#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QDebug>

// ---------------------------------------------------------------------------
// UdpEndpoint implementation
// ---------------------------------------------------------------------------

UdpEndpoint::UdpEndpoint(int connectionId, QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_connectionId(connectionId)
{
    connect(m_socket, &QUdpSocket::readyRead,
            this, &UdpEndpoint::onReadyRead);

    // error() перегружен в Qt5 — используем старый синтаксис
    // как во всех остальных сетевых классах проекта
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
                const QString errStr = m_socket->errorString();
                qWarning() << "[UdpEndpoint] id=" << m_connectionId
                           << "socket error:" << errStr;

                emit messageReceived(Message::system(
                    m_connectionId,
                    Message::Protocol::Udp,
                    QString("Error: %1").arg(errStr)
                    ));
                emit errorOccurred(m_connectionId, errStr);
            });
#else
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif
}

UdpEndpoint::~UdpEndpoint()
{
    // QUdpSocket удалится автоматически (parent = this)
    // Явно закрываем чтобы не получить readyRead во время разрушения
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
    }
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

UdpEndpoint::State UdpEndpoint::state() const
{
    return m_state;
}

int UdpEndpoint::connectionId() const
{
    return m_connectionId;
}

quint16 UdpEndpoint::localPort() const
{
    return m_socket->localPort();
}

QHostAddress UdpEndpoint::localAddress() const
{
    return m_socket->localAddress();
}

QString UdpEndpoint::targetAddress() const
{
    return m_targetAddress;
}

quint16 UdpEndpoint::targetPort() const
{
    return m_targetPort;
}

bool UdpEndpoint::isBroadcastEnabled() const
{
    return m_broadcastEnabled;
}

// ---------------------------------------------------------------------------
// Настройка
// ---------------------------------------------------------------------------

void UdpEndpoint::setTargetAddress(const QString &address)
{
    m_targetAddress = address;
}

void UdpEndpoint::setTargetPort(quint16 port)
{
    m_targetPort = port;
}

void UdpEndpoint::setBroadcastEnabled(bool enabled)
{
    m_broadcastEnabled = enabled;
    // Qt включает SO_BROADCAST автоматически когда writeDatagram()
    // вызывается с адресом 255.255.255.255 — явная установка не нужна
    // m_broadcastEnabled используется только как UI-флаг
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

bool UdpEndpoint::bind(const QHostAddress &address, quint16 port)
{
    // Если уже привязан — освобождаем перед повторным bind
    if (m_state == State::Bound) {
        qDebug() << "[UdpEndpoint] id=" << m_connectionId
                 << "rebinding socket";
        m_socket->close();
        setState(State::Unbound);
    }

    // ShareAddress позволяет нескольким сокетам слушать один порт —
    // полезно для мультикаста и broadcast получения
    // ReuseAddressHint — не блокировать порт после закрытия (TIME_WAIT)
    const bool bound = m_socket->bind(
        address,
        port,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint
        );

    if (!bound) {
        const QString errStr = m_socket->errorString();
        qWarning() << "[UdpEndpoint] id=" << m_connectionId
                   << "bind failed:" << errStr;

        emit messageReceived(Message::system(
            m_connectionId,
            Message::Protocol::Udp,
            QString("Bind error: %1").arg(errStr)
            ));
        emit errorOccurred(m_connectionId, errStr);
        return false;
    }

    setState(State::Bound);

    qDebug() << "[UdpEndpoint] id=" << m_connectionId
             << "bound to" << m_socket->localAddress().toString()
             << "port" << m_socket->localPort();

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Udp,
        QString("Bound to %1:%2")
            .arg(m_socket->localAddress().toString())
            .arg(m_socket->localPort())
        ));

    return true;
}

void UdpEndpoint::unbind()
{
    if (m_state == State::Unbound) {
        return;
    }

    m_socket->close();
    setState(State::Unbound);

    qDebug() << "[UdpEndpoint] id=" << m_connectionId << "unbound";

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Udp,
        QStringLiteral("Unbound")
        ));
}

bool UdpEndpoint::sendData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return false;
    }

    if (m_targetAddress.isEmpty()) {
        qWarning() << "[UdpEndpoint] id=" << m_connectionId
                   << "sendData: target address not set";
        return false;
    }

    if (m_targetPort == 0) {
        qWarning() << "[UdpEndpoint] id=" << m_connectionId
                   << "sendData: target port not set";
        return false;
    }

    const QHostAddress target(m_targetAddress);

    if (target.isNull()) {
        const QString errStr =
            QString("Invalid target address: %1").arg(m_targetAddress);
        qWarning() << "[UdpEndpoint] id=" << m_connectionId << errStr;
        emit errorOccurred(m_connectionId, errStr);
        return false;
    }

    const qint64 written = m_socket->writeDatagram(data, target, m_targetPort);

    if (written == -1) {
        const QString errStr = m_socket->errorString();
        qWarning() << "[UdpEndpoint] id=" << m_connectionId
                   << "writeDatagram failed:" << errStr;
        emit errorOccurred(m_connectionId, errStr);
        return false;
    }

    emit messageReceived(Message::outgoing(
        m_connectionId,
        Message::Protocol::Udp,
        data,
        false   // UDP — бинарный, isText определяет получатель
        ));

    return true;
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void UdpEndpoint::onReadyRead()
{
    // Читаем ВСЕ доступные датаграммы за один вызов слота —
    // readyRead() может прийти один раз для нескольких датаграмм
    while (m_socket->hasPendingDatagrams()) {

        // receiveDatagram() возвращает QNetworkDatagram с адресом отправителя
        // Альтернатива — readDatagram() — не даёт адрес на некоторых платформах
        const QNetworkDatagram datagram = m_socket->receiveDatagram();

        if (datagram.isNull()) {
            continue;
        }

        const QByteArray     payload       = datagram.data();
        const QHostAddress   senderAddress = datagram.senderAddress();
        const quint16        senderPort    =
            static_cast<quint16>(datagram.senderPort());

        qDebug() << "[UdpEndpoint] id=" << m_connectionId
                 << "datagram from" << senderAddress.toString()
                 << "port" << senderPort
                 << "size" << payload.size();

        // messageReceived — стандартный поток для MessageLogModel
        emit messageReceived(Message::incoming(
            m_connectionId,
            Message::Protocol::Udp,
            payload,
            false
            ));

        // datagramReceived — расширенный сигнал для UI панели
        // (показывает адрес отправителя отдельно от лога)
        emit datagramReceived(
            m_connectionId,
            senderAddress,
            senderPort,
            payload
            );
    }
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

void UdpEndpoint::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_connectionId, m_state);
}