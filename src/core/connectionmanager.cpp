#include "ConnectionManager.h"

#include <QDebug>

// ---------------------------------------------------------------------------
// ConnectionManager implementation
// ---------------------------------------------------------------------------

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
{
}

ConnectionManager::~ConnectionManager()
{
    // removeAll() корректно закрывает все соединения перед удалением
    // QMap хранит объекты с parent = this, но сетевые классы
    // должны быть закрыты до удаления — иначе получим сигналы
    // во время разрушения объекта
    removeAll();
}

// ---------------------------------------------------------------------------
// Фабричные методы
// ---------------------------------------------------------------------------

int ConnectionManager::createTcpClient()
{
    const int id = nextId();

    TcpClient *client = new TcpClient(id, this);
    m_tcpClients.insert(id, client);

    connectTcpClientSignals(client);

    ConnectionInfo info;
    info.id          = id;
    info.type        = ConnectionInfo::Type::TcpClient;
    info.displayName = QString("TCP Client #%1").arg(id);
    info.isActive    = false;
    m_infos.insert(id, info);

    ConnectionStats s;
    s.connectedAt = QDateTime::currentDateTimeUtc();
    m_stats.insert(id, s);

    qDebug() << "[ConnectionManager] created TcpClient id=" << id;

    emit connectionAdded(info);
    return id;
}

int ConnectionManager::createTcpServer()
{
    const int id = nextId();

    TcpServer *server = new TcpServer(id, this);
    m_tcpServers.insert(id, server);

    connectTcpServerSignals(server);

    ConnectionInfo info;
    info.id          = id;
    info.type        = ConnectionInfo::Type::TcpServer;
    info.displayName = QString("TCP Server #%1").arg(id);
    info.isActive    = false;
    m_infos.insert(id, info);

    ConnectionStats s;
    s.connectedAt = QDateTime::currentDateTimeUtc();
    m_stats.insert(id, s);

    qDebug() << "[ConnectionManager] created TcpServer id=" << id;

    emit connectionAdded(info);
    return id;
}

int ConnectionManager::createWsClient()
{
    const int id = nextId();

    WsClient *client = new WsClient(id, this);
    m_wsClients.insert(id, client);

    connectWsClientSignals(client);

    ConnectionInfo info;
    info.id          = id;
    info.type        = ConnectionInfo::Type::WebSocket;
    info.displayName = QString("WebSocket #%1").arg(id);
    info.isActive    = false;
    m_infos.insert(id, info);

    ConnectionStats s;
    s.connectedAt = QDateTime::currentDateTimeUtc();
    m_stats.insert(id, s);

    qDebug() << "[ConnectionManager] created WsClient id=" << id;

    emit connectionAdded(info);
    return id;
}

int ConnectionManager::createWsServer()
{
    const int id = nextId();
    WsServer *server = new WsServer(id, this);
    m_wsServers.insert(id, server);
    connectWsServerSignals(server);

    ConnectionInfo info;
    info.id          = id;
    info.type        = ConnectionInfo::Type::WsServer;
    info.displayName = QString("WS Server #%1").arg(id);
    info.isActive    = false;
    m_infos.insert(id, info);

    ConnectionStats s;
    s.connectedAt = QDateTime::currentDateTimeUtc();
    m_stats.insert(id, s);

    emit connectionAdded(info);
    return id;
}

int ConnectionManager::createUdpEndpoint()
{
    const int id = nextId();

    UdpEndpoint *endpoint = new UdpEndpoint(id, this);
    m_udpEndpoints.insert(id, endpoint);

    connectUdpEndpointSignals(endpoint);

    ConnectionInfo info;
    info.id          = id;
    info.type        = ConnectionInfo::Type::Udp;
    info.displayName = QString("UDP #%1").arg(id);
    info.isActive    = false;
    m_infos.insert(id, info);

    // Инициализация статистики — как в других create* методах
    ConnectionStats s;
    s.connectedAt = QDateTime::currentDateTimeUtc();
    m_stats.insert(id, s);

    qDebug() << "[ConnectionManager] created UdpEndpoint id=" << id;

    emit connectionAdded(info);
    return id;
}

// ---------------------------------------------------------------------------
// Управление соединениями
// ---------------------------------------------------------------------------

bool ConnectionManager::connectTcpClient(int id,
                                         const QString &host,
                                         quint16 port)
{
    if (!m_tcpClients.contains(id)) {
        qWarning() << "[ConnectionManager] connectTcpClient: unknown id" << id;
        return false;
    }

    // Обновляем displayName с реальным адресом до подключения —
    // UI увидит адрес сразу, не дожидаясь сигнала connected()
    if (m_infos.contains(id)) {
        m_infos[id].displayName =
            QString("TCP Client #%1 — %2:%3").arg(id).arg(host).arg(port);
        emit connectionInfoChanged(id);
    }

    m_tcpClients[id]->connectToHost(host, port);
    return true;
}

bool ConnectionManager::disconnectTcpClient(int id)
{
    if (!m_tcpClients.contains(id)) {
        qWarning() << "[ConnectionManager] disconnectTcpClient: unknown id" << id;
        return false;
    }

    m_tcpClients[id]->disconnectFromHost();
    return true;
}

bool ConnectionManager::startTcpServer(int id,
                                       const QHostAddress &address,
                                       quint16 port)
{
    if (!m_tcpServers.contains(id)) {
        qWarning() << "[ConnectionManager] startTcpServer: unknown id" << id;
        return false;
    }

    return m_tcpServers[id]->startListening(address, port);
}

bool ConnectionManager::stopTcpServer(int id)
{
    if (!m_tcpServers.contains(id)) {
        qWarning() << "[ConnectionManager] stopTcpServer: unknown id" << id;
        return false;
    }

    m_tcpServers[id]->stopListening();
    return true;
}

bool ConnectionManager::connectWsClient(int id, const QUrl &url)
{
    if (!m_wsClients.contains(id)) {
        qWarning() << "[ConnectionManager] connectWsClient: unknown id" << id;
        return false;
    }

    if (m_infos.contains(id)) {
        m_infos[id].displayName =
            QString("WebSocket #%1 — %2").arg(id).arg(url.toString());
        emit connectionInfoChanged(id);
    }

    m_wsClients[id]->connectToUrl(url);
    return true;
}

bool ConnectionManager::disconnectWsClient(int id)
{
    if (!m_wsClients.contains(id)) {
        qWarning() << "[ConnectionManager] disconnectWsClient: unknown id" << id;
        return false;
    }

    m_wsClients[id]->disconnectFromHost();
    return true;
}

bool ConnectionManager::startWsServer(int id,
                                      const QHostAddress &address,
                                      quint16 port)
{
    if (!m_wsServers.contains(id)) return false;
    return m_wsServers[id]->startListening(address, port);
}

bool ConnectionManager::stopWsServer(int id)
{
    if (!m_wsServers.contains(id)) return false;
    m_wsServers[id]->stopListening();
    return true;
}

// ---------------------------------------------------------------------------
// Отправка данных
// ---------------------------------------------------------------------------

bool ConnectionManager::sendToTcpClient(int id, const QByteArray &data)
{
    if (!m_tcpClients.contains(id)) {
        qWarning() << "[ConnectionManager] sendToTcpClient: unknown id" << id;
        return false;
    }
    return m_tcpClients[id]->sendData(data);
}

bool ConnectionManager::sendToTcpServerClient(int id,
                                              qintptr descriptor,
                                              const QByteArray &data)
{
    if (!m_tcpServers.contains(id)) {
        qWarning() << "[ConnectionManager] sendToTcpServerClient: unknown id" << id;
        return false;
    }
    return m_tcpServers[id]->sendToClient(descriptor, data);
}

bool ConnectionManager::broadcastTcpServer(int id, const QByteArray &data)
{
    if (!m_tcpServers.contains(id)) {
        qWarning() << "[ConnectionManager] broadcastTcpServer: unknown id" << id;
        return false;
    }
    m_tcpServers[id]->broadcast(data);
    return true;
}

bool ConnectionManager::sendWsText(int id, const QString &text)
{
    if (!m_wsClients.contains(id)) {
        qWarning() << "[ConnectionManager] sendWsText: unknown id" << id;
        return false;
    }
    return m_wsClients[id]->sendTextMessage(text);
}

bool ConnectionManager::sendWsBinary(int id, const QByteArray &data)
{
    if (!m_wsClients.contains(id)) {
        qWarning() << "[ConnectionManager] sendWsBinary: unknown id" << id;
        return false;
    }
    return m_wsClients[id]->sendBinaryMessage(data);
}

bool ConnectionManager::sendWsTextToSession(int id,
                                            int sessionId,
                                            const QString &text)
{
    if (!m_wsServers.contains(id)) return false;
    return m_wsServers[id]->sendTextToClient(sessionId, text);
}

bool ConnectionManager::sendWsBinaryToSession(int id,
                                              int sessionId,
                                              const QByteArray &data)
{
    if (!m_wsServers.contains(id)) return false;
    return m_wsServers[id]->sendBinaryToClient(sessionId, data);
}

bool ConnectionManager::broadcastWsText(int id, const QString &text)
{
    if (!m_wsServers.contains(id)) return false;
    m_wsServers[id]->broadcastText(text);
    return true;
}

bool ConnectionManager::broadcastWsBinary(int id, const QByteArray &data)
{
    if (!m_wsServers.contains(id)) return false;
    m_wsServers[id]->broadcastBinary(data);
    return true;
}

bool ConnectionManager::bindUdpEndpoint(int id,
                                        const QHostAddress &address,
                                        quint16 port)
{
    if (!m_udpEndpoints.contains(id)) {
        qWarning() << "[ConnectionManager] bindUdpEndpoint: unknown id" << id;
        return false;
    }

    // Обновляем displayName с портом до bind — UI увидит его сразу
    if (m_infos.contains(id)) {
        m_infos[id].displayName =
            QString("UDP #%1 — %2:%3")
                .arg(id)
                .arg(address == QHostAddress::Any ? "0.0.0.0"
                                                  : address.toString())
                .arg(port);
        emit connectionInfoChanged(id);
    }

    return m_udpEndpoints[id]->bind(address, port);
}

bool ConnectionManager::unbindUdpEndpoint(int id)
{
    if (!m_udpEndpoints.contains(id)) {
        qWarning() << "[ConnectionManager] unbindUdpEndpoint: unknown id" << id;
        return false;
    }
    m_udpEndpoints[id]->unbind();
    return true;
}

void ConnectionManager::setUdpTarget(int id,
                                     const QString &address,
                                     quint16 port)
{
    if (!m_udpEndpoints.contains(id)) {
        return;
    }
    m_udpEndpoints[id]->setTargetAddress(address);
    m_udpEndpoints[id]->setTargetPort(port);
}

void ConnectionManager::setUdpBroadcast(int id, bool enabled)
{
    if (m_udpEndpoints.contains(id)) {
        m_udpEndpoints[id]->setBroadcastEnabled(enabled);
    }
}

bool ConnectionManager::sendUdpData(int id, const QByteArray &data)
{
    if (!m_udpEndpoints.contains(id)) {
        qWarning() << "[ConnectionManager] sendUdpData: unknown id" << id;
        return false;
    }
    return m_udpEndpoints[id]->sendData(data);
}

// ---------------------------------------------------------------------------
// Настройка параметров
// ---------------------------------------------------------------------------

void ConnectionManager::setTcpClientReconnectInterval(int id, int ms)
{
    if (m_tcpClients.contains(id)) {
        m_tcpClients[id]->setReconnectInterval(ms);
    }
}

void ConnectionManager::setWsClientPingInterval(int id, int ms)
{
    if (m_wsClients.contains(id)) {
        m_wsClients[id]->setPingInterval(ms);
    }
}

void ConnectionManager::setWsClientReconnectInterval(int id, int ms)
{
    if (m_wsClients.contains(id)) {
        m_wsClients[id]->setReconnectInterval(ms);
    }
}

// ---------------------------------------------------------------------------
// Удаление
// ---------------------------------------------------------------------------

bool ConnectionManager::removeConnection(int id)
{
    if (!m_infos.contains(id)) {
        qWarning() << "[ConnectionManager] removeConnection: unknown id" << id;
        return false;
    }

    // Закрываем соединение перед удалением объекта
    if (m_tcpClients.contains(id)) {
        m_tcpClients[id]->disconnectFromHost();
        // deleteLater() — объект может быть в стеке сигналов в этот момент
        m_tcpClients[id]->deleteLater();
        m_tcpClients.remove(id);
    }
    else if (m_tcpServers.contains(id)) {
        m_tcpServers[id]->stopListening();
        m_tcpServers[id]->deleteLater();
        m_tcpServers.remove(id);
    }
    else if (m_wsClients.contains(id)) {
        m_wsClients[id]->disconnectFromHost();
        m_wsClients[id]->deleteLater();
        m_wsClients.remove(id);
    }
    else if (m_wsServers.contains(id)) {
        m_wsServers[id]->stopListening();
        m_wsServers[id]->deleteLater();
        m_wsServers.remove(id);
    }
    else if (m_udpEndpoints.contains(id)) {
        m_udpEndpoints[id]->unbind();
        m_udpEndpoints[id]->deleteLater();
        m_udpEndpoints.remove(id);
    }

    m_infos.remove(id);
    m_stats.remove(id);
    m_lastSpeedSampleTime.remove(id);
    m_bytesInSinceLastSample.remove(id);
    m_bytesOutSinceLastSample.remove(id);

    qDebug() << "[ConnectionManager] removed connection id=" << id;

    emit connectionRemoved(id);
    return true;
}

void ConnectionManager::removeAll()
{
    // Копируем ключи — removeConnection() модифицирует m_infos
    const QList<int> ids = m_infos.keys();
    for (int id : ids) {
        removeConnection(id);
    }
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

QList<ConnectionInfo> ConnectionManager::connections() const
{
    return m_infos.values();
}

ConnectionInfo ConnectionManager::connectionInfo(int id) const
{
    return m_infos.value(id, ConnectionInfo());
}

bool ConnectionManager::hasTcpClient(int id)   const { return m_tcpClients.contains(id);   }
bool ConnectionManager::hasTcpServer(int id)   const { return m_tcpServers.contains(id);   }
bool ConnectionManager::hasWsClient (int id)   const { return m_wsClients.contains(id);    }
bool ConnectionManager::hasWsServer (int id)   const { return m_wsServers.contains(id);    }
bool ConnectionManager::hasUdpEndpoint(int id) const { return m_udpEndpoints.contains(id); }

QList<ClientSession> ConnectionManager::tcpServerSessions(int id) const
{
    if (!m_tcpServers.contains(id)) return {};
    return m_tcpServers.value(id)->sessions();
}

QList<WsClientSession> ConnectionManager::wsServerSessions(int id) const
{
    if (!m_wsServers.contains(id)) return {};
    return m_wsServers.value(id)->sessions();
}

// ---------------------------------------------------------------------------
// Приватные слоты — агрегаторы сигналов
// ---------------------------------------------------------------------------

void ConnectionManager::onTcpClientConnected(int id)
{
    updateActiveState(id, true);
}

void ConnectionManager::onTcpClientDisconnected(int id)
{
    updateActiveState(id, false);
}

void ConnectionManager::onTcpServerListeningChanged(bool listening)
{
    // sender() здесь — TcpServer Получаем id через его connectionId()
    TcpServer *server = qobject_cast<TcpServer *>(sender());
    if (!server) {
        return;
    }
    updateActiveState(server->connectionId(), listening);
}

void ConnectionManager::onWsClientConnected(int id)
{
    updateActiveState(id, true);
}

void ConnectionManager::onWsClientDisconnected(int id)
{
    updateActiveState(id, false);
}

void ConnectionManager::onWsServerListeningChanged(bool listening)
{
    WsServer *server = qobject_cast<WsServer *>(sender());
    if (server) {
        const int id = server->connectionId();
        // Обновляем displayName с реальным портом когда сервер стартовал
        if (listening && m_infos.contains(id)) {
            m_infos[id].displayName =
                QString("WS Server #%1 — ws://0.0.0.0:%2")
                    .arg(id).arg(server->port());
            emit connectionInfoChanged(id);
        }
        updateActiveState(id, listening);
    }
}

void ConnectionManager::onMessageReceived(const Message &message)
{
    updateStats(message);
    // Прозрачный проброс — менеджер не модифицирует сообщения
    emit messageReceived(message);
}

// TCP Server — проброс клиентских событий
void ConnectionManager::onServerClientConnected(qintptr descriptor,
                                                const QString &displayName)
{
    // sender() возвращает объект который испустил сигнал — это TcpServer
    TcpServer *srv = qobject_cast<TcpServer *>(sender());
    if (!srv) {
        return;
    }
    emit serverClientConnected(srv->connectionId(), descriptor, displayName);
}

void ConnectionManager::onServerClientDisconnected(qintptr descriptor,
                                                   const QString &displayName)
{
    TcpServer *srv = qobject_cast<TcpServer *>(sender());
    if (!srv) {
        return;
    }
    emit serverClientDisconnected(srv->connectionId(), descriptor, displayName);
}

// WS Server — проброс клиентских событий
void ConnectionManager::onWsServerClientConnected(int sessionId,
                                                  const QString &displayName)
{
    WsServer *srv = qobject_cast<WsServer *>(sender());
    if (!srv) return;
    emit wsServerClientConnected(srv->connectionId(), sessionId, displayName);
}

void ConnectionManager::onWsServerClientDisconnected(int sessionId,
                                                     const QString &displayName)
{
    WsServer *srv = qobject_cast<WsServer *>(sender());
    if (!srv) return;
    emit wsServerClientDisconnected(srv->connectionId(), sessionId, displayName);
}

void ConnectionManager::onUdpStateChanged(int id, UdpEndpoint::State state)
{
    // Bound = активен (принимает датаграммы), Unbound = неактивен
    updateActiveState(id, state == UdpEndpoint::State::Bound);
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

int ConnectionManager::nextId()
{
    return m_nextId++;
}

void ConnectionManager::connectTcpClientSignals(TcpClient *client)
{
    connect(client, SIGNAL(connected(int)),
            this, SLOT(onTcpClientConnected(int)));

    connect(client, SIGNAL(disconnected(int)),
            this, SLOT(onTcpClientDisconnected(int)));

    connect(client, SIGNAL(messageReceived(Message)),
            this, SLOT(onMessageReceived(Message)));

    // errorOccurred пробрасываем напрямую — у него тот же сигнал что и у нас
    // используем лямбду через QMetaObject чтобы не добавлять лишний слот
    connect(client, SIGNAL(errorOccurred(int, QString)),
            this, SIGNAL(connectionInfoChanged(int)));
}

void ConnectionManager::connectTcpServerSignals(TcpServer *server)
{
    connect(server, SIGNAL(listeningChanged(bool)),
            this, SLOT(onTcpServerListeningChanged(bool)));

    connect(server, SIGNAL(messageReceived(Message)),
            this, SLOT(onMessageReceived(Message)));

    connect(server, SIGNAL(clientConnected(qintptr, QString)),
            this, SLOT(onServerClientConnected(qintptr, QString)));

    connect(server, SIGNAL(clientDisconnected(qintptr, QString)),
            this, SLOT(onServerClientDisconnected(qintptr, QString)));
}

void ConnectionManager::connectWsClientSignals(WsClient *client)
{
    connect(client, SIGNAL(connected(int)),
            this, SLOT(onWsClientConnected(int)));

    connect(client, SIGNAL(disconnected(int)),
            this, SLOT(onWsClientDisconnected(int)));

    connect(client, SIGNAL(messageReceived(Message)),
            this, SLOT(onMessageReceived(Message)));
}

void ConnectionManager::connectWsServerSignals(WsServer *server)
{
    connect(server, SIGNAL(listeningChanged(bool)),
            this, SLOT(onWsServerListeningChanged(bool)));

    connect(server, SIGNAL(messageReceived(Message)),
            this, SLOT(onMessageReceived(Message)));

    connect(server, SIGNAL(clientConnected(int, QString)),
            this, SLOT(onWsServerClientConnected(int, QString)));

    connect(server, SIGNAL(clientDisconnected(int, QString)),
            this, SLOT(onWsServerClientDisconnected(int, QString)));
}

void ConnectionManager::connectUdpEndpointSignals(UdpEndpoint *endpoint)
{
    connect(endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)),
            this, SLOT(onUdpStateChanged(int, UdpEndpoint::State)));

    connect(endpoint, SIGNAL(messageReceived(Message)),
            this, SLOT(onMessageReceived(Message)));

    // errorOccurred пробрасываем напрямую — тот же паттерн что у TcpClient
    connect(endpoint, SIGNAL(errorOccurred(int, QString)),
            this, SIGNAL(connectionInfoChanged(int)));
}

void ConnectionManager::updateActiveState(int id, bool active)
{
    if (!m_infos.contains(id)) {
        return;
    }

    // Обновляем только если состояние изменилось — не спамим сигналами
    if (m_infos[id].isActive == active) {
        return;
    }

    m_infos[id].isActive = active;
    emit connectionInfoChanged(id);
}

ConnectionStats ConnectionManager::stats(int id) const
{
    return m_stats.value(id, ConnectionStats());
}

void ConnectionManager::resetStats(int id)
{
    if (!m_stats.contains(id)) {
        return;
    }

    m_stats[id].reset();
    m_bytesInSinceLastSample[id]  = 0;
    m_bytesOutSinceLastSample[id] = 0;

    emit statsUpdated(id, m_stats[id]);
}

void ConnectionManager::updateStats(const Message &message)
{
    const int id = message.connectionId;

    if (!m_stats.contains(id)) {
        return;
    }

    ConnectionStats &s = m_stats[id];
    const quint64 size = static_cast<quint64>(message.payload.size());

    // Обновляем счётчики по направлению
    if (message.direction == Message::Direction::Incoming) {
        s.bytesIn    += size;
        s.messagesIn += 1;
        m_bytesInSinceLastSample[id] += size;
    } else if (message.direction == Message::Direction::Outgoing) {
        s.bytesOut    += size;
        s.messagesOut += 1;
        m_bytesOutSinceLastSample[id] += size;
    }

    s.lastActivityAt = QDateTime::currentDateTimeUtc();

    // ---------------------------------------------------------------------------
    // Расчёт пиковой скорости.
    // Замеряем раз в секунду — если прошло >= 1000 мс с последнего замера,
    // вычисляем скорость за интервал и обновляем peak если больше текущего.
    // ---------------------------------------------------------------------------
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (!m_lastSpeedSampleTime.contains(id)) {
        m_lastSpeedSampleTime[id]       = now;
        m_bytesInSinceLastSample[id]    = 0;
        m_bytesOutSinceLastSample[id]   = 0;
    }

    const qint64 msSinceLastSample =
        m_lastSpeedSampleTime[id].msecsTo(now);

    if (msSinceLastSample >= 1000) {
        // байт/сек за прошедший интервал
        const double secElapsed = msSinceLastSample / 1000.0;

        const quint64 speedIn  = static_cast<quint64>(
            m_bytesInSinceLastSample[id]  / secElapsed
            );
        const quint64 speedOut = static_cast<quint64>(
            m_bytesOutSinceLastSample[id] / secElapsed
            );

        if (speedIn  > s.peakBytesPerSecIn)  s.peakBytesPerSecIn  = speedIn;
        if (speedOut > s.peakBytesPerSecOut) s.peakBytesPerSecOut = speedOut;

        // Сброс накопителей для следующего интервала
        m_lastSpeedSampleTime[id]       = now;
        m_bytesInSinceLastSample[id]    = 0;
        m_bytesOutSinceLastSample[id]   = 0;
    }

    emit statsUpdated(id, s);
}
