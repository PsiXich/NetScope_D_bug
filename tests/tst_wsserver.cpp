#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>

#include "core/WsServer.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// WsTestClient — вспомогательный WebSocket-клиент для тестов
//
// Обёртка над QWebSocket — упрощает подключение и отправку данных
// Живёт только в этом файле, аналог EchoServer в tst_tcpclient.cpp
// ---------------------------------------------------------------------------
class WsTestClient : public QObject
{
    Q_OBJECT

public:
    explicit WsTestClient(QObject *parent = nullptr)
        : QObject(parent)
        , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    {
        connect(m_socket, &QWebSocket::connected,
                this, &WsTestClient::connected);

        connect(m_socket, &QWebSocket::disconnected,
                this, &WsTestClient::disconnected);

        connect(m_socket, &QWebSocket::textMessageReceived,
                this, &WsTestClient::textReceived);

        connect(m_socket, &QWebSocket::binaryMessageReceived,
                this, &WsTestClient::binaryReceived);
    }

    void connectTo(quint16 port)
    {
        m_socket->open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));
    }

    void disconnectFromServer()
    {
        m_socket->close();
    }

    void sendText(const QString &text)
    {
        m_socket->sendTextMessage(text);
    }

    void sendBinary(const QByteArray &data)
    {
        m_socket->sendBinaryMessage(data);
    }

    bool isConnected() const
    {
        return m_socket->state() == QAbstractSocket::ConnectedState;
    }

    QString lastTextReceived;
    QByteArray lastBinaryReceived;

signals:
    void connected();
    void disconnected();
    void textReceived(const QString &message);
    void binaryReceived(const QByteArray &message);

private:
    QWebSocket *m_socket;
};

// ---------------------------------------------------------------------------
// WsServerTest — набор тестов
// ---------------------------------------------------------------------------
class WsServerTest : public QObject
{
    Q_OBJECT

private slots:
    // Жизненный цикл: init() перед каждым тестом, cleanup() после
    void init();
    void cleanup();

    // --- Базовое состояние ---
    void testInitialState();

    // --- Управление сервером ---
    void testStartListening();
    void testStopListening();
    void testStartOnUnavailablePort();
    void testDoubleStart();

    // --- Клиентские соединения ---
    void testClientConnect();
    void testClientDisconnect();
    void testMultipleClients();

    // --- Передача данных ---
    void testReceiveTextFromClient();
    void testReceiveBinaryFromClient();
    void testSendTextToClient();
    void testSendBinaryToClient();
    void testBroadcastText();
    void testBroadcastBinary();

    // --- Граничные случаи ---
    void testSendToUnknownSession();
    void testStopWithActiveClients();

private:
    WsServer    *m_server { nullptr };
    static const int CONNECTION_ID = 42;
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void WsServerTest::init()
{
    m_server = new WsServer(CONNECTION_ID, this);
}

void WsServerTest::cleanup()
{
    delete m_server;
    m_server = nullptr;
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void WsServerTest::testInitialState()
{
    QCOMPARE(m_server->isListening(), false);
    QCOMPARE(m_server->connectionId(), CONNECTION_ID);
    QCOMPARE(m_server->clientCount(), 0);
    QCOMPARE(m_server->port(), quint16(0));
    QVERIFY(m_server->sessions().isEmpty());
}

void WsServerTest::testStartListening()
{
    QSignalSpy spyListening(m_server, SIGNAL(listeningChanged(bool)));
    QSignalSpy spyMessage (m_server, SIGNAL(messageReceived(Message)));

    // port = 0 — ОС выбирает свободный порт
    const bool started = m_server->startListening(QHostAddress::LocalHost, 0);

    QVERIFY(started);
    QVERIFY(m_server->isListening());
    QVERIFY(m_server->port() > 0);     // ОС назначила реальный порт

    // Должны получить сигнал listeningChanged(true) и системное сообщение
    QCOMPARE(spyListening.count(), 1);
    QCOMPARE(spyListening.first().at(0).toBool(), true);

    QCOMPARE(spyMessage.count(), 1);
    const Message msg = spyMessage.first().at(0).value<Message>();
    QCOMPARE(msg.direction, Message::Direction::System);
    QCOMPARE(msg.protocol,  Message::Protocol::WsServer);
}

void WsServerTest::testStopListening()
{
    m_server->startListening(QHostAddress::LocalHost, 0);
    QVERIFY(m_server->isListening());

    QSignalSpy spyListening(m_server, SIGNAL(listeningChanged(bool)));

    m_server->stopListening();

    QCOMPARE(m_server->isListening(), false);
    QCOMPARE(spyListening.count(), 1);
    QCOMPARE(spyListening.first().at(0).toBool(), false);
}

void WsServerTest::testStartOnUnavailablePort()
{
    QSignalSpy spyError(m_server, SIGNAL(errorOccurred(int, QString)));

    // Порт 1 — зарезервирован, bind должен упасть
    const bool started = m_server->startListening(QHostAddress::LocalHost, 1);

    QCOMPARE(started, false);
    QCOMPARE(m_server->isListening(), false);
    QVERIFY(spyError.count() > 0);

    const int    id  = spyError.first().at(0).toInt();
    const QString err = spyError.first().at(1).toString();
    QCOMPARE(id, CONNECTION_ID);
    QVERIFY(!err.isEmpty());
}

void WsServerTest::testDoubleStart()
{
    // Повторный вызов startListening() когда уже слушаем — должен вернуть true
    // и не запустить ещё один сервер
    m_server->startListening(QHostAddress::LocalHost, 0);
    const quint16 firstPort = m_server->port();

    QSignalSpy spyListening(m_server, SIGNAL(listeningChanged(bool)));

    const bool result = m_server->startListening(QHostAddress::LocalHost, 0);

    QVERIFY(result);
    QCOMPARE(m_server->port(), firstPort);    // порт не изменился
    QCOMPARE(spyListening.count(), 0);        // нет лишних сигналов
}

void WsServerTest::testClientConnect()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(int, QString)));

    WsTestClient client;
    QSignalSpy spyClientConnected(&client, SIGNAL(connected()));

    client.connectTo(m_server->port());

    // Ждём пока клиент установит соединение
    QVERIFY(spyClientConnected.wait(3000));
    // Ждём пока сервер обработает новое соединение
    if (spyConnected.isEmpty()) {
        QVERIFY(spyConnected.wait(2000));
    }

    QCOMPARE(m_server->clientCount(), 1);
    QCOMPARE(spyConnected.count(), 1);

    // Проверяем аргументы сигнала
    const int     sessionId   = spyConnected.first().at(0).toInt();
    const QString displayName = spyConnected.first().at(1).toString();

    QVERIFY(sessionId >= 0);
    QVERIFY(!displayName.isEmpty());

    // Сессия должна быть в списке
    QCOMPARE(m_server->sessions().size(), 1);
    QCOMPARE(m_server->sessions().first().id, sessionId);
}

void WsServerTest::testClientDisconnect()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyClientConn(&client, SIGNAL(connected()));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(int, QString)));

    client.connectTo(m_server->port());
    QVERIFY(spyClientConn.wait(3000));

    // Клиент закрывает соединение
    client.disconnectFromServer();

    if (spyDisconnected.isEmpty()) {
        QVERIFY(spyDisconnected.wait(3000));
    }

    QCOMPARE(m_server->clientCount(), 0);
    QVERIFY(m_server->sessions().isEmpty());
}

void WsServerTest::testMultipleClients()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(int, QString)));

    WsTestClient client1;
    WsTestClient client2;
    WsTestClient client3;

    QSignalSpy spy1(&client1, SIGNAL(connected()));
    QSignalSpy spy2(&client2, SIGNAL(connected()));
    QSignalSpy spy3(&client3, SIGNAL(connected()));

    client1.connectTo(m_server->port());
    client2.connectTo(m_server->port());
    client3.connectTo(m_server->port());

    QVERIFY(spy1.wait(3000));
    QVERIFY(spy2.wait(2000));
    QVERIFY(spy3.wait(2000));

    // Ждём пока сервер зарегистрирует всех клиентов
    const int maxWait = 2000;
    const int step    = 50;
    int elapsed       = 0;
    while (m_server->clientCount() < 3 && elapsed < maxWait) {
        QTest::qWait(step);
        elapsed += step;
    }

    QCOMPARE(m_server->clientCount(), 3);
    QCOMPARE(spyConnected.count(), 3);

    // id сессий должны быть уникальными
    const QList<WsClientSession> sessions = m_server->sessions();
    QSet<int> ids;
    for (const WsClientSession &s : sessions) {
        ids.insert(s.id);
    }
    QCOMPARE(ids.size(), 3);
}

void WsServerTest::testReceiveTextFromClient()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyClientConn(&client, SIGNAL(connected()));
    QSignalSpy spyMessage(m_server, SIGNAL(messageReceived(Message)));

    client.connectTo(m_server->port());
    QVERIFY(spyClientConn.wait(3000));

    // Ждём системное сообщение о подключении
    if (spyMessage.isEmpty()) {
        spyMessage.wait(500);
    }
    spyMessage.clear();

    // Клиент отправляет text frame
    const QString text = "Hello WsServer!";
    client.sendText(text);

    QVERIFY(spyMessage.wait(2000));

    // Ищем входящее text сообщение
    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming
            && msg.isText
            && QString::fromUtf8(msg.payload) == text) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Did not receive expected text message from client");
}

void WsServerTest::testReceiveBinaryFromClient()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyClientConn(&client, SIGNAL(connected()));
    QSignalSpy spyMessage(m_server, SIGNAL(messageReceived(Message)));

    client.connectTo(m_server->port());
    QVERIFY(spyClientConn.wait(3000));

    if (spyMessage.isEmpty()) {
        spyMessage.wait(500);
    }
    spyMessage.clear();

    // Клиент отправляет binary frame
    const QByteArray data = QByteArray::fromHex("DEADBEEF");
    client.sendBinary(data);

    QVERIFY(spyMessage.wait(2000));

    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming
            && !msg.isText
            && msg.payload == data) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Did not receive expected binary message from client");
}

void WsServerTest::testSendTextToClient()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyClientConn(&client, SIGNAL(connected()));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(int, QString)));
    QSignalSpy spyTextReceived(&client, SIGNAL(textReceived(QString)));

    client.connectTo(m_server->port());
    QVERIFY(spyClientConn.wait(3000));
    if (spyConnected.isEmpty()) {
        QVERIFY(spyConnected.wait(2000));
    }

    const int     sessionId = spyConnected.first().at(0).toInt();
    const QString text      = "Hello from server!";

    const bool sent = m_server->sendTextToClient(sessionId, text);
    QVERIFY(sent);

    // Клиент должен получить text frame
    QVERIFY(spyTextReceived.wait(2000));
    QCOMPARE(spyTextReceived.first().at(0).toString(), text);
}

void WsServerTest::testSendBinaryToClient()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyClientConn(&client, SIGNAL(connected()));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(int, QString)));
    QSignalSpy spyBinaryReceived(&client, SIGNAL(binaryReceived(QByteArray)));

    client.connectTo(m_server->port());
    QVERIFY(spyClientConn.wait(3000));
    if (spyConnected.isEmpty()) {
        QVERIFY(spyConnected.wait(2000));
    }

    const int        sessionId = spyConnected.first().at(0).toInt();
    const QByteArray data      = QByteArray::fromHex("CAFEBABE");

    const bool sent = m_server->sendBinaryToClient(sessionId, data);
    QVERIFY(sent);

    QVERIFY(spyBinaryReceived.wait(2000));
    QCOMPARE(spyBinaryReceived.first().at(0).toByteArray(), data);
}

void WsServerTest::testBroadcastText()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client1;
    WsTestClient client2;
    QSignalSpy spy1Conn(&client1, SIGNAL(connected()));
    QSignalSpy spy2Conn(&client2, SIGNAL(connected()));
    QSignalSpy spy1Text(&client1, SIGNAL(textReceived(QString)));
    QSignalSpy spy2Text(&client2, SIGNAL(textReceived(QString)));

    client1.connectTo(m_server->port());
    client2.connectTo(m_server->port());
    QVERIFY(spy1Conn.wait(3000));
    QVERIFY(spy2Conn.wait(2000));

    // Ждём регистрации обоих клиентов
    const int maxWait = 2000;
    const int step    = 50;
    int elapsed       = 0;
    while (m_server->clientCount() < 2 && elapsed < maxWait) {
        QTest::qWait(step);
        elapsed += step;
    }
    QCOMPARE(m_server->clientCount(), 2);

    const QString text = "Broadcast!";
    m_server->broadcastText(text);

    // Оба клиента должны получить сообщение
    QVERIFY(spy1Text.wait(2000));
    QVERIFY(spy2Text.count() > 0 || spy2Text.wait(2000));

    QCOMPARE(spy1Text.first().at(0).toString(), text);
    QCOMPARE(spy2Text.first().at(0).toString(), text);
}

void WsServerTest::testBroadcastBinary()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client1;
    WsTestClient client2;
    QSignalSpy spy1Conn(&client1, SIGNAL(connected()));
    QSignalSpy spy2Conn(&client2, SIGNAL(connected()));
    QSignalSpy spy1Bin(&client1, SIGNAL(binaryReceived(QByteArray)));
    QSignalSpy spy2Bin(&client2, SIGNAL(binaryReceived(QByteArray)));

    client1.connectTo(m_server->port());
    client2.connectTo(m_server->port());
    QVERIFY(spy1Conn.wait(3000));
    QVERIFY(spy2Conn.wait(2000));

    const int maxWait = 2000;
    const int step    = 50;
    int elapsed       = 0;
    while (m_server->clientCount() < 2 && elapsed < maxWait) {
        QTest::qWait(step);
        elapsed += step;
    }

    const QByteArray data = QByteArray::fromHex("0102030405");
    m_server->broadcastBinary(data);

    QVERIFY(spy1Bin.wait(2000));
    QVERIFY(spy2Bin.count() > 0 || spy2Bin.wait(2000));

    QCOMPARE(spy1Bin.first().at(0).toByteArray(), data);
    QCOMPARE(spy2Bin.first().at(0).toByteArray(), data);
}

void WsServerTest::testSendToUnknownSession()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    // Отправка на несуществующий sessionId должна вернуть false
    // и не крашиться
    const bool result = m_server->sendTextToClient(999, "test");
    QCOMPARE(result, false);

    const bool result2 = m_server->sendBinaryToClient(999, QByteArray("test"));
    QCOMPARE(result2, false);
}

void WsServerTest::testStopWithActiveClients()
{
    m_server->startListening(QHostAddress::LocalHost, 0);

    WsTestClient client;
    QSignalSpy spyConn(&client, SIGNAL(connected()));
    QSignalSpy spyServerListening(m_server, SIGNAL(listeningChanged(bool)));

    client.connectTo(m_server->port());
    QVERIFY(spyConn.wait(3000));

    const int maxWait = 2000;
    const int step    = 50;
    int elapsed       = 0;
    while (m_server->clientCount() < 1 && elapsed < maxWait) {
        QTest::qWait(step);
        elapsed += step;
    }
    QCOMPARE(m_server->clientCount(), 1);

    // Останавливаем сервер с активным клиентом — не должно крашиться
    m_server->stopListening();

    QCOMPARE(m_server->isListening(), false);
    QCOMPARE(m_server->clientCount(), 0);

    if (spyServerListening.isEmpty()) {
        QVERIFY(spyServerListening.wait(1000));
    }
    QCOMPARE(spyServerListening.last().at(0).toBool(), false);
}

// ---------------------------------------------------------------------------
// Точка входа
// ---------------------------------------------------------------------------
QTEST_MAIN(WsServerTest)

#include "tst_wsserver.moc"