#include <QTest>
#include <QSignalSpy>

#include "core/ConnectionManager.h"

// Регистрация типа для QSignalSpy
Q_DECLARE_METATYPE(ConnectionInfo)

// ---------------------------------------------------------------------------
// ConnectionManagerTest
// ---------------------------------------------------------------------------
class ConnectionManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    // --- Тесты ---
    void testInitialState();
    void testCreateConnections();
    void testRemoveConnection();
    void testRemoveAll();
    void testInvalidIdHandling();
    void testConnectionInfoUpdates();

private:
    ConnectionManager *m_manager { nullptr };
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void ConnectionManagerTest::initTestCase()
{
    // Регистрируем структуру чтобы QSignalSpy мог её распаковать
    qRegisterMetaType<ConnectionInfo>("ConnectionInfo");
}

void ConnectionManagerTest::init()
{
    m_manager = new ConnectionManager(this);
}

void ConnectionManagerTest::cleanup()
{
    delete m_manager;
    m_manager = nullptr;
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void ConnectionManagerTest::testInitialState()
{
    QVERIFY(m_manager->connections().isEmpty());
    QCOMPARE(m_manager->hasTcpClient(0), false);
    QCOMPARE(m_manager->hasTcpServer(0), false);
    QCOMPARE(m_manager->hasWsClient(0), false);

    // Запрос информации по несуществующему ID должен вернуть пустой объект (-1)
    const ConnectionInfo info = m_manager->connectionInfo(999);
    QCOMPARE(info.id, -1);
}

void ConnectionManagerTest::testCreateConnections()
{
    QSignalSpy spyAdded(m_manager, SIGNAL(connectionAdded(ConnectionInfo)));

    // Создаем TCP Client
    const int tcpClientId = m_manager->createTcpClient();
    QVERIFY(m_manager->hasTcpClient(tcpClientId));
    QCOMPARE(m_manager->connections().size(), 1);

    // Проверяем сигнал
    QCOMPARE(spyAdded.count(), 1);
    ConnectionInfo infoClient = spyAdded.takeFirst().at(0).value<ConnectionInfo>();
    QCOMPARE(infoClient.id, tcpClientId);
    QCOMPARE(infoClient.type, ConnectionInfo::Type::TcpClient);

    // Создаем TCP Server
    const int tcpServerId = m_manager->createTcpServer();
    QVERIFY(m_manager->hasTcpServer(tcpServerId));
    QCOMPARE(m_manager->connections().size(), 2);

    // Проверяем сигнал
    QCOMPARE(spyAdded.count(), 1);
    ConnectionInfo infoServer = spyAdded.takeFirst().at(0).value<ConnectionInfo>();
    QCOMPARE(infoServer.id, tcpServerId);
    QCOMPARE(infoServer.type, ConnectionInfo::Type::TcpServer);

    // Создаем WebSocket Client
    const int wsClientId = m_manager->createWsClient();
    QVERIFY(m_manager->hasWsClient(wsClientId));
    QCOMPARE(m_manager->connections().size(), 3);

    // Убеждаемся, что ID уникальны
    QVERIFY(tcpClientId != tcpServerId);
    QVERIFY(tcpServerId != wsClientId);
}

void ConnectionManagerTest::testRemoveConnection()
{
    QSignalSpy spyRemoved(m_manager, SIGNAL(connectionRemoved(int)));

    const int id = m_manager->createTcpClient();
    QVERIFY(m_manager->hasTcpClient(id));

    const bool removed = m_manager->removeConnection(id);
    QVERIFY(removed);
    QVERIFY(!m_manager->hasTcpClient(id));
    QVERIFY(m_manager->connections().isEmpty());

    QCOMPARE(spyRemoved.count(), 1);
    QCOMPARE(spyRemoved.first().at(0).toInt(), id);
}

void ConnectionManagerTest::testRemoveAll()
{
    QSignalSpy spyRemoved(m_manager, SIGNAL(connectionRemoved(int)));

    m_manager->createTcpClient();
    m_manager->createTcpServer();
    m_manager->createWsClient();

    QCOMPARE(m_manager->connections().size(), 3);

    m_manager->removeAll();

    QVERIFY(m_manager->connections().isEmpty());
    QCOMPARE(spyRemoved.count(), 3); // Должно прийти 3 сигнала об удалении
}

void ConnectionManagerTest::testInvalidIdHandling()
{
    const int badId = 999;

    // Вызовы с несуществующим ID должны возвращать false и не крашить приложение
    QVERIFY(!m_manager->connectTcpClient(badId, "127.0.0.1", 80));
    QVERIFY(!m_manager->disconnectTcpClient(badId));
    QVERIFY(!m_manager->startTcpServer(badId));
    QVERIFY(!m_manager->stopTcpServer(badId));
    QVERIFY(!m_manager->connectWsClient(badId, QUrl("ws://localhost")));
    QVERIFY(!m_manager->disconnectWsClient(badId));

    QVERIFY(!m_manager->sendToTcpClient(badId, "data"));
    QVERIFY(!m_manager->broadcastTcpServer(badId, "data"));
    QVERIFY(!m_manager->sendWsText(badId, "data"));

    QVERIFY(!m_manager->removeConnection(badId));
}

void ConnectionManagerTest::testConnectionInfoUpdates()
{
    QSignalSpy spyInfoChanged(m_manager, SIGNAL(connectionInfoChanged(int)));

    const int id = m_manager->createTcpServer();
    spyInfoChanged.clear(); // Очищаем от возможных сигналов при инициализации

    // Запускаем сервер, это должно изменить его состояние на isActive = true
    // и вызвать сигнал connectionInfoChanged
    m_manager->startTcpServer(id, QHostAddress::LocalHost, 0);

    // Ждем, пока состояние обновится (через локальный EventLoop)
    // Так как QTcpServer запускается синхронно, сигнал должен быть в очереди
    QTest::qWait(50);

    QVERIFY(spyInfoChanged.count() > 0);

    const ConnectionInfo info = m_manager->connectionInfo(id);
    QCOMPARE(info.isActive, true);

    m_manager->stopTcpServer(id);
}

// ---------------------------------------------------------------------------
// Точка входа QTest
// ---------------------------------------------------------------------------
QTEST_MAIN(ConnectionManagerTest)

#include "tst_connectionmanager.moc"