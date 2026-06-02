#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>

#include "core/TcpClient.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// EchoServer — вспомогательный TCP-сервер для тестов
//
// Живёт только в этом файле — не выносим в helpers/ пока он не нужен
// в других тестах. Если понадобится в tst_TcpServer.cpp — переносим тогда
//
// Поведение: принимает одно соединение, всё полученное отправляет обратно
// ---------------------------------------------------------------------------
class EchoServer : public QObject
{
    Q_OBJECT

public:
    explicit EchoServer(QObject *parent = nullptr)
        : QObject(parent)
        , m_server(new QTcpServer(this))
    {
        connect(m_server, SIGNAL(newConnection()),
                this, SLOT(onNewConnection()));
    }

    // Запускаем на случайном свободном порту (port = 0)
    bool listen()
    {
        return m_server->listen(QHostAddress::LocalHost, 0);
    }

    quint16 port() const
    {
        return m_server->serverPort();
    }

    int connectionsCount() const
    {
        return m_connectionsCount;
    }

    void disconnectAll()
    {
        const QList<QTcpSocket*> clientsCopy = m_clients;
        for (QTcpSocket *client : clientsCopy) {
            // Проверяем, что сокет ещё жив и подключён
            if (client && client->state() == QAbstractSocket::ConnectedState) {
                client->abort();
            }
        }
    }

private slots:
    void onNewConnection()
    {
        while (m_server->hasPendingConnections()) {
            QTcpSocket *socket = m_server->nextPendingConnection();
            m_clients.append(socket);
            m_connectionsCount++;

            connect(socket, &QTcpSocket::readyRead, [socket]() {
                socket->write(socket->readAll());
            });

            // ПРАВИЛЬНАЯ ОЧИСТКА ПРИ ОТКЛЮЧЕНИИ
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                m_clients.removeOne(socket); // Удаляем из списка живых клиентов
                m_connectionsCount--;
                socket->deleteLater();       // Затем безопасно удаляем объект
            });
        }
    }

    void onReadyRead()
    {
        QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
        if (!client) return;

        const QByteArray data = client->readAll();
        client->write(data);   // echo
    }

private:
    QTcpServer          *m_server;
    QList<QTcpSocket *>  m_clients;
    int                  m_connectionsCount { 0 };
};

// ---------------------------------------------------------------------------
// TcpClientTest — набор тестов
// ---------------------------------------------------------------------------
class TcpClientTest : public QObject
{
    Q_OBJECT

private slots:
    // --- Методы жизненного цикла QTest ---
    // init() вызывается перед каждым тестом — чистое состояние
    void init();
    // cleanup() вызывается после каждого теста — освобождаем ресурсы
    void cleanup();

    // --- Тесты ---
    void testInitialState();
    void testConnectToHost();
    void testDisconnectFromHost();
    void testSendAndReceiveData();
    void testSendDataWhenDisconnected();
    void testStateChangedSignal();
    void testReconnect();
    void testConnectToUnavailableHost();

private:
    // Вспомогательный метод: ждём сигнал с таймаутом
    // Возвращает true если сигнал пришёл в течение timeoutMs
    bool waitForSignal(QObject *sender, const char *signal, int timeoutMs = 2000);

    EchoServer  *m_server   { nullptr };
    TcpClient   *m_client   { nullptr };

    static const int CONNECTION_ID = 1;
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void TcpClientTest::init()
{
    m_server = new EchoServer(this);
    QVERIFY2(m_server->listen(), "EchoServer failed to start");

    m_client = new TcpClient(CONNECTION_ID, this);
}

void TcpClientTest::cleanup()
{
    // Сначала клиент — чтобы не получить сигналы после удаления сервера
    delete m_client;
    m_client = nullptr;

    delete m_server;
    m_server = nullptr;
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void TcpClientTest::testInitialState()
{
    // Только что созданный клиент должен быть в отключённом состоянии
    QCOMPARE(m_client->state(), TcpClient::State::Disconnected);
    QCOMPARE(m_client->connectionId(), CONNECTION_ID);
    QCOMPARE(m_client->reconnectInterval(), 0);
    QVERIFY(m_client->host().isEmpty());
    QCOMPARE(m_client->port(), quint16(0));
}

void TcpClientTest::testConnectToHost()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyState(m_client, SIGNAL(stateChanged(int, TcpClient::State)));

    m_client->connectToHost("127.0.0.1", m_server->port());

    // Сразу после вызова должны быть в состоянии Connecting
    QCOMPARE(m_client->state(), TcpClient::State::Connecting);

    // Ждём сигнал connected()
    QVERIFY(spyConnected.wait(2000));

    QCOMPARE(m_client->state(), TcpClient::State::Connected);
    QCOMPARE(m_client->host(), QString("127.0.0.1"));
    QCOMPARE(m_client->port(), m_server->port());

    // Должно быть минимум 2 изменения состояния: Connecting → Connected
    QVERIFY(spyState.count() >= 2);
}

void TcpClientTest::testDisconnectFromHost()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));

    m_client->connectToHost("127.0.0.1", m_server->port());
    QVERIFY(spyConnected.wait(2000));

    m_client->disconnectFromHost();

    // Даём время event loop обработать disconnect
    if (spyDisconnected.isEmpty()) {
        QVERIFY(spyDisconnected.wait(3000));
    }

    QVERIFY(spyDisconnected.count() > 0);
    QCOMPARE(m_client->state(), TcpClient::State::Disconnected);
}

void TcpClientTest::testSendAndReceiveData()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyMessage(m_client, SIGNAL(messageReceived(Message)));

    m_client->connectToHost("127.0.0.1", m_server->port());
    QVERIFY(spyConnected.wait(2000));

    const QByteArray payload = "Hello, NetScope!";
    const bool sent = m_client->sendData(payload);
    QVERIFY(sent);

    // Ждём входящее сообщение (эхо от сервера)
    // spyMessage уже содержит системное сообщение "Connected" и
    // исходящее сообщение — ждём пока не появится входящее.
    // Используем wait() в цикле чтобы не зависеть от порядка сигналов
    const int maxWaitMs  = 2000;
    const int stepMs     = 50;
    int       elapsed    = 0;
    bool      gotIncoming = false;

    while (elapsed < maxWaitMs) {
        QTest::qWait(stepMs);
        elapsed += stepMs;

        for (int i = 0; i < spyMessage.count(); ++i) {
            const Message msg = spyMessage.at(i).at(0).value<Message>();
            if (msg.direction == Message::Direction::Incoming
                && msg.payload == payload) {
                gotIncoming = true;
                break;
            }
        }
        if (gotIncoming) break;
    }

    QVERIFY2(gotIncoming, "Did not receive echo from server within timeout");
}

void TcpClientTest::testSendDataWhenDisconnected()
{
    // sendData() на отключённом клиенте должен вернуть false
    // и не крашиться
    const bool result = m_client->sendData("test");
    QCOMPARE(result, false);
}

void TcpClientTest::testStateChangedSignal()
{
    QSignalSpy spyState(m_client, SIGNAL(stateChanged(int, TcpClient::State)));
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));

    m_client->connectToHost("127.0.0.1", m_server->port());

    // Ждем подключения
    if (spyConnected.isEmpty()) {
        QVERIFY(spyConnected.wait(2000));
    }

    spyState.clear();

    m_client->disconnectFromHost();
    if (spyDisconnected.isEmpty()) {
        QVERIFY(spyDisconnected.wait(3000));
    }

    QCOMPARE(m_client->state(), TcpClient::State::Disconnected);
    // обязательно проверяем, что сигнал изменения статуса действительно отправлялся
    QVERIFY(spyState.count() > 0);
}

void TcpClientTest::testReconnect()
{
    m_client->setReconnectInterval(300);

    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyState(m_client, SIGNAL(stateChanged(int, TcpClient::State)));

    m_client->connectToHost("127.0.0.1", m_server->port());
    QVERIFY(spyConnected.wait(2000));
    QCOMPARE(spyConnected.count(), 1);

    // Гарантируем, что сервер успел отреагировать на новое подключение
    // и положил сокет в свой список m_clients.
    QTRY_VERIFY_WITH_TIMEOUT(m_server->connectionsCount() > 0, 2000);
    // ---------------------------

    qDebug() << "[TEST] Before disconnectAll - state:" << m_client->state();

    m_server->disconnectAll();

    // Ждём перехода в Disconnected
    bool connectionLost = false;
    for (int i = 0; i < 120; ++i) {   // до 6 секунд
        if (m_client->state() == TcpClient::State::Disconnected) {
            connectionLost = true;
            break;
        }
        QTest::qWait(50);
    }

    qDebug() << "[TEST] After wait - final state:" << m_client->state();

    QVERIFY2(connectionLost, "Client did not go to Disconnected state");

    // Ждём реконнект
    QVERIFY2(spyConnected.wait(6000), "Reconnect did not happen");
    QCOMPARE(spyConnected.count(), 2);
    QCOMPARE(m_client->state(), TcpClient::State::Connected);
}

void TcpClientTest::testConnectToUnavailableHost()
{
    QSignalSpy spyError(m_client, SIGNAL(errorOccurred(int, QString)));
    QSignalSpy spyState(m_client, SIGNAL(stateChanged(int, TcpClient::State)));

    m_client->connectToHost("127.0.0.1", 1);  // заведомо недоступный порт

    // Ждём либо error, либо disconnected
    QVERIFY(spyError.wait(5000) || spyState.wait(3000));

    QCOMPARE(m_client->state(), TcpClient::State::Disconnected);

    // Проверяем, что ошибка была
    QVERIFY(spyError.count() > 0);
}

bool TcpClientTest::waitForSignal(QObject *sender, const char *signal, int timeoutMs)
{
    QSignalSpy spy(sender, signal);
    return spy.wait(timeoutMs);
}

// ---------------------------------------------------------------------------
// Точка входа QTest
// QTEST_MAIN разворачивается в main() с QApplication внутри
// Для сетевых тестов нужен event loop — QTEST_GUILESS_MAIN не подходит
// ---------------------------------------------------------------------------
QTEST_MAIN(TcpClientTest)

#include "tst_tcpclient.moc"