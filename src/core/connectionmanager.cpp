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

    qDebug() << "[ConnectionManager] created WsClient id=" << id;

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

QList<ClientSession> ConnectionManager::tcpServerSessions(int id) const
{
    if (!m_tcpServers.contains(id)) {
        return QList<ClientSession>();
    }
    return m_tcpServers.value(id)->sessions();
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

    m_infos.remove(id);

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

bool ConnectionManager::hasTcpClient(int id) const
{
    return m_tcpClients.contains(id);
}

bool ConnectionManager::hasTcpServer(int id) const
{
    return m_tcpServers.contains(id);
}

bool ConnectionManager::hasWsClient(int id) const
{
    return m_wsClients.contains(id);
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

void ConnectionManager::onMessageReceived(const Message &message)
{
    // Прозрачный проброс — менеджер не модифицирует сообщения
    emit messageReceived(message);
}

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