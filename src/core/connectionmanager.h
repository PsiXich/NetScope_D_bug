#ifndef NETSCOPE_CONNECTIONMANAGER_H
#define NETSCOPE_CONNECTIONMANAGER_H

#include "Message.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "WsClient.h"
#include "WsServer.h"
#include "UdpEndpoint.h"
#include "ConnectionStats.h"

#include <QObject>
#include <QMap>

// ---------------------------------------------------------------------------
// ConnectionInfo — метаданные о соединении для отображения в UI
//
// Plain struct — не хранит указатели на сетевые объекты,
// только то что нужно UI для отображения списка соединений
// Сетевые объекты живут в приватных QMap внутри ConnectionManager
// ---------------------------------------------------------------------------
struct ConnectionInfo
{
    enum class Type {
        TcpClient,
        TcpServer,
        WebSocket,
        WsServer,
        Udp
    };

    int     id          { -1 };
    Type    type        { Type::TcpClient };
    QString displayName;        // "TCP Client: 127.0.0.1:8080"
    bool    isActive    { false };

    // Тип в виде строки — для лога и UI-лейблов
    QString typeString() const
    {
        switch (type) {
        case Type::TcpClient:  return QStringLiteral("TCP Client");
        case Type::TcpServer:  return QStringLiteral("TCP Server");
        case Type::WebSocket:  return QStringLiteral("WebSocket");
        case Type::WsServer:   return QStringLiteral("WS Server");
        case Type::Udp:        return QStringLiteral("UDP");
        }
        return QStringLiteral("Unknown");
    }
};

// ---------------------------------------------------------------------------
// ConnectionManager — единственное место где создаются и хранятся
// все сетевые объекты приложения
//
// Ответственность:
//   - создавать TcpClient / TcpServer / WsClient с уникальными id
//   - агрегировать сигналы всех соединений в единый поток сигналов
//   - предоставлять UI список соединений через ConnectionInfo
//   - удалять соединения и освобождать ресурсы
//
// Паттерн: Facade + Registry.
//   UI работает только с ConnectionManager, не с сетевыми классами напрямую
//   Это единственная точка входа для всей сетевой логики
//
// todo: добавление нового протокола (UDP, MQTT) =
//   новый класс + новый метод create*() здесь + новый тип в ConnectionInfo
// ---------------------------------------------------------------------------
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager() override;

    // --- Фабричные методы ---
    // Возвращают id созданного соединения
    // Объект создаётся и сразу регистрируется — id валиден до removeConnection()

    // Создать TCP-клиент Не подключается автоматически —
    // вызови connectTcpClient() после настройки параметров
    int createTcpClient();

    // Создать TCP-сервер Не начинает слушать автоматически
    int createTcpServer();

    // Создать WebSocket-клиен. Не подключается автоматически
    int createWsClient();

    // Создать WebSocket-сервер
    int createWsServer();

    // UDP
    int createUdpEndpoint();

    // Bind на локальный порт для приёма датаграмм
    bool bindUdpEndpoint(int id,
                         const QHostAddress &address = QHostAddress::Any,
                         quint16 port = 0);
    // Освободить порт
    bool unbindUdpEndpoint(int id);

    // Настройка цели отправки — независимо от bind
    void setUdpTarget(int id,
                      const QString &address,
                      quint16 port);

    // Включить broadcast (255.255.255.255)
    void setUdpBroadcast(int id, bool enabled);

    // Отправить датаграмму
    bool sendUdpData(int id, const QByteArray &data);

    // --- Управление соединениями ---

    // Подключить TCP-клиент к хосту
    bool connectTcpClient(int id, const QString &host, quint16 port);

    // Отключить TCP-клиент
    bool disconnectTcpClient(int id);

    // Запустить TCP-сервер
    bool startTcpServer(int id,
                        const QHostAddress &address = QHostAddress::Any,
                        quint16 port = 0);

    // Остановить TCP-сервер
    bool stopTcpServer(int id);

    // Подключить WebSocket-клиент
    bool connectWsClient(int id, const QUrl &url);

    // Отключить WebSocket-клиент
    bool disconnectWsClient(int id);

    // Запустить WebSocket-сервер
    bool startWsServer(int id,
                       const QHostAddress &address = QHostAddress::Any,
                       quint16 port = 0);

    // Отключить WebSocket-сервер
    bool stopWsServer(int id);

    // --- Отправка данных ---

    bool sendToTcpClient(int id, const QByteArray &data);
    bool sendToTcpServerClient(int id, qintptr descriptor, const QByteArray &data);
    bool broadcastTcpServer(int id, const QByteArray &data);
    bool sendWsText(int id, const QString &text);
    bool sendWsBinary(int id, const QByteArray &data);

    // WsServer: отправка конкретному клиенту
    bool sendWsTextToSession  (int id, int sessionId, const QString &text);
    bool sendWsBinaryToSession(int id, int sessionId, const QByteArray &data);
    // WsServer: broadcast всем клиентам сервера
    bool broadcastWsText  (int id, const QString &text);
    bool broadcastWsBinary(int id, const QByteArray &data);

    // --- Настройка параметров ---

    void setTcpClientReconnectInterval(int id, int ms);
    void setWsClientPingInterval(int id, int ms);
    void setWsClientReconnectInterval(int id, int ms);

    // --- Удаление ---

    // Удалить соединение по id Корректно закрывает перед удалением
    bool removeConnection(int id);

    // Удалить все соединения
    void removeAll();

    // --- Запросы состояния ---

    // Список всех зарегистрированных соединений для UI
    QList<ConnectionInfo> connections() const;

    // Информация о конкретном соединении
    ConnectionInfo connectionInfo(int id) const;

    bool hasTcpClient(int id)   const;
    bool hasTcpServer(int id)   const;
    bool hasWsClient(int id)    const;
    bool hasWsServer (int id)   const;
    bool hasUdpEndpoint(int id) const;

    // Прямой доступ к сессиям сервера для обновления UI
    QList<ClientSession> tcpServerSessions(int id) const;
    QList<WsClientSession> wsServerSessions (int id) const;

    // Статистика соединения. Возвращает дефолтный ConnectionStats
    // если id не найден — не крашится при некорректном id
    ConnectionStats stats(int id) const;

    // Сбросить статистику конкретного соединения
    void resetStats(int id);

signals:
    // Все сообщения от всех соединений — единый поток для MessageLogModel
    void messageReceived(const Message &message);

    // Соединение добавлено / удалено — для ConnectionListModel
    void connectionAdded(const ConnectionInfo &info);
    void connectionRemoved(int id);

    // Мета-изменения конкретных соединений — UI обновляет статус
    void connectionInfoChanged(int id);

    // Обновление статистики — эмитируется после каждого сообщения
    // StatsWidget подписывается для live-обновления без polling
    void statsUpdated(int id, const ConnectionStats &stats);

    // Проброс клиентских событий TCP-сервера наружу
    // serverId — id в ConnectionManager, не дескриптор сокета
    void serverClientConnected   (int serverId,
                               qintptr descriptor,
                               const QString &displayName);
    void serverClientDisconnected(int serverId,
                                  qintptr descriptor,
                                  const QString &displayName);

    // --- WS Server клиентские события ---
    void wsServerClientConnected   (int serverId,
                                 int sessionId,
                                 const QString &displayName);
    void wsServerClientDisconnected(int serverId,
                                    int sessionId,
                                    const QString &displayName);

private slots:
    // Агрегаторы сигналов от сетевых объектов
    void onTcpClientConnected(int id);
    void onTcpClientDisconnected(int id);
    void onTcpServerListeningChanged(bool listening);
    void onWsClientConnected(int id);
    void onWsClientDisconnected(int id);
    void onUdpStateChanged(int id, UdpEndpoint::State state);
    void onWsServerListeningChanged(bool listening);

    // Единый обработчик messageReceived от всех типов
    void onMessageReceived(const Message &message);

    // TCP Server — проброс клиентских событий
    void onServerClientConnected   (qintptr descriptor,
                                 const QString &displayName);
    void onServerClientDisconnected(qintptr descriptor,
                                    const QString &displayName);

    // WS Server — проброс клиентских событий
    void onWsServerClientConnected   (int sessionId,
                                   const QString &displayName);
    void onWsServerClientDisconnected(int sessionId,
                                      const QString &displayName);

private:
    // Генератор уникальных id — простой инкремент
    int nextId();

    // Подключение сигналов от TcpClient к слотам менеджера
    void connectTcpClientSignals(TcpClient *client);

    // Подключение сигналов от TcpServer
    void connectTcpServerSignals(TcpServer *server);

    // Подключение сигналов от WsClient
    void connectWsClientSignals(WsClient *client);

    // Подключение сигналов от WsServer
    void connectWsServerSignals (WsServer  *server);

    // Подключение сигналов UDP
    void connectUdpEndpointSignals(UdpEndpoint *endpoint);

    // Обновить ConnectionInfo.isActive и emit connectionInfoChanged()
    void updateActiveState(int id, bool active);

    // Обновить счётчики статистики по входящему/исходящему сообщению
    void updateStats(const Message &message);

    // Реестры объектов — владеем через QMap + parent (QObject)
    QMap<int, TcpClient *>   m_tcpClients;
    QMap<int, TcpServer *>   m_tcpServers;
    QMap<int, WsClient  *>   m_wsClients;
    QMap<int, WsServer  *>   m_wsServers;
    QMap<int, UdpEndpoint *> m_udpEndpoints;

    // Метаданные для UI — отдельно от объектов чтобы не тянуть
    // сетевые заголовки туда где нужна только отображаемая информация
    QMap<int, ConnectionInfo>  m_infos;
    QMap<int, ConnectionStats> m_stats;

    // Вспомогательные данные для расчёта скорости:
    // время последнего замера и накопленные байты за интервал
    QMap<int, QDateTime> m_lastSpeedSampleTime;
    QMap<int, quint64>   m_bytesInSinceLastSample;
    QMap<int, quint64>   m_bytesOutSinceLastSample;

    int m_nextId { 0 };
};

#endif // NETSCOPE_CONNECTIONMANAGER_H
