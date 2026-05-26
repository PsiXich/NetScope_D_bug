#include <QTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/WsClient.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// WsEchoServer — вспомогательный WebSocket-сервер для тестов
// ---------------------------------------------------------------------------
class WsEchoServer : public QObject
{
    Q_OBJECT

public:
    explicit WsEchoServer(QObject *parent = nullptr)
        : QObject(parent)
        , m_server(new QWebSocketServer("TestServer", QWebSocketServer::NonSecureMode, this))
    {
        connect(m_server, SIGNAL(newConnection()),
                this, SLOT(onNewConnection()));
    }

    // Запускаем на случайном свободном порту (port = 0)
    bool listen()
    {
        return m_server->listen(QHostAddress::LocalHost, 0);
    }

    QUrl serverUrl() const
    {
        return m_server->serverUrl();
    }

    void disconnectAll()
    {
        for (QWebSocket *client : std::as_const(m_clients)) {
            client->close();
        }
    }

private slots:
    void onNewConnection()
    {
        while (m_server->hasPendingConnections()) {
            QWebSocket *client = m_server->nextPendingConnection();
            m_clients.append(client);

            connect(client, SIGNAL(textMessageReceived(QString)),
                    this, SLOT(onTextMessageReceived(QString)));

            connect(client, SIGNAL(binaryMessageReceived(QByteArray)),
                    this, SLOT(onBinaryMessageReceived(QByteArray)));

            connect(client, SIGNAL(disconnected()),
                    client, SLOT(deleteLater()));
        }
    }

    void onTextMessageReceived(const QString &message)
    {
        QWebSocket *client = qobject_cast<QWebSocket *>(sender());
        if (client) {
            client->sendTextMessage(message); // Эхо
        }
    }

    void onBinaryMessageReceived(const QByteArray &message)
    {
        QWebSocket *client = qobject_cast<QWebSocket *>(sender());
        if (client) {
            client->sendBinaryMessage(message); // Эхо
        }
    }

private:
    QWebSocketServer    *m_server;
    QList<QWebSocket *>  m_clients;
};

// ---------------------------------------------------------------------------
// WsClientTest
// ---------------------------------------------------------------------------
class WsClientTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- Тесты ---
    void testInitialState();
    void testConnectAndDisconnect();
    void testSendAndReceiveText();
    void testSendAndReceiveBinary();
    void testPingPong();
    void testReconnect();
    void testConnectToUnavailableUrl();

private:
    WsEchoServer *m_server { nullptr };
    WsClient     *m_client { nullptr };

    static const int CONNECTION_ID = 55;
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void WsClientTest::init()
{
    m_server = new WsEchoServer(this);
    QVERIFY2(m_server->listen(), "WsEchoServer failed to start");

    m_client = new WsClient(CONNECTION_ID, this);
}

void WsClientTest::cleanup()
{
    delete m_client;
    m_client = nullptr;

    delete m_server;
    m_server = nullptr;
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void WsClientTest::testInitialState()
{
    // Только что созданный клиент должен быть в отключённом состоянии
    QCOMPARE(m_client->state(), WsClient::State::Disconnected);
    QCOMPARE(m_client->connectionId(), CONNECTION_ID);
    QCOMPARE(m_client->pingInterval(), 0);
    QCOMPARE(m_client->reconnectInterval(), 0);
    QVERIFY(m_client->url().isEmpty());
}

void WsClientTest::testConnectAndDisconnect()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));
    QSignalSpy spyState(m_client, SIGNAL(stateChanged(int, WsClient::State)));

    m_client->connectToUrl(m_server->serverUrl());
    // Сразу после вызова должны быть в состоянии Connecting
    QCOMPARE(m_client->state(), WsClient::State::Connecting);

    QVERIFY(spyConnected.wait(2000));
    QCOMPARE(m_client->state(), WsClient::State::Connected);
    QCOMPARE(m_client->url(), m_server->serverUrl());

    spyState.clear();

    m_client->disconnectFromHost();
    // Ждём если сигнал прилетел не синхронно
    if (spyDisconnected.isEmpty()) {
        spyDisconnected.wait(2000);
    }
    // Явно проверяем, что сигнал всё-таки пришел
    QVERIFY2(spyDisconnected.count() > 0, "Signal disconnected() was not emitted");
    QCOMPARE(m_client->state(), WsClient::State::Disconnected);
    QVERIFY(spyState.count() > 0);
}

void WsClientTest::testSendAndReceiveText()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyMessage(m_client, SIGNAL(messageReceived(Message)));

    m_client->connectToUrl(m_server->serverUrl());
    QVERIFY(spyConnected.wait(2000));

    spyMessage.clear(); // Очищаем от системного сообщения о подключении

    const QString textPayload = "Hello, WebSocket!";
    QVERIFY(m_client->sendTextMessage(textPayload));

    // Ждем два сообщения: первое — исходящее (Outgoing), второе — эхо (Incoming)
    while (spyMessage.count() < 2) {
        QVERIFY(spyMessage.wait(2000));
    }

    // Извлекаем входящее сообщение
    Message incomingMsg;
    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming) {
            incomingMsg = msg;
            found = true;
            break;
        }
    }

    QVERIFY(found);
    QCOMPARE(incomingMsg.isText, true);
    QCOMPARE(QString::fromUtf8(incomingMsg.payload), textPayload);
}

void WsClientTest::testSendAndReceiveBinary()
{
    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyMessage(m_client, SIGNAL(messageReceived(Message)));

    m_client->connectToUrl(m_server->serverUrl());
    QVERIFY(spyConnected.wait(2000));

    spyMessage.clear();

    const QByteArray binPayload = QByteArray::fromHex("DEADBEEF");
    QVERIFY(m_client->sendBinaryMessage(binPayload));

    while (spyMessage.count() < 2) {
        QVERIFY(spyMessage.wait(2000));
    }

    Message incomingMsg;
    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming) {
            incomingMsg = msg;
            found = true;
            break;
        }
    }

    QVERIFY(found);
    QCOMPARE(incomingMsg.isText, false);
    QCOMPARE(incomingMsg.payload, binPayload);
}

void WsClientTest::testPingPong()
{
    // Настраиваем пинг каждые 200мс (только для тестов, в реале нужно больше)
    m_client->setPingInterval(200);

    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyPong(m_client, SIGNAL(pongReceived(int,quint64,QByteArray)));

    m_client->connectToUrl(m_server->serverUrl());
    QVERIFY(spyConnected.wait(2000));

    // QWebSocketServer автоматически отвечает на Ping
    // Ждем хотя бы один Pong от сервера
    QVERIFY(spyPong.wait(2000));
    QVERIFY(spyPong.count() > 0);
}

void WsClientTest::testReconnect()
{
    m_client->setReconnectInterval(300);

    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));
    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));

    m_client->connectToUrl(m_server->serverUrl());
    QVERIFY(spyConnected.wait(2000));
    QCOMPARE(spyConnected.count(), 1);

    // Имитируем обрыв со стороны сервера
    m_server->disconnectAll();
    QVERIFY(spyDisconnected.wait(2000));

    // Ждем автопереподключение
    QVERIFY(spyConnected.wait(3000));
    // Счетчик connected должен стать 2 (второе подключение)
    QCOMPARE(spyConnected.count(), 2);
    QCOMPARE(m_client->state(), WsClient::State::Connected);
}

void WsClientTest::testConnectToUnavailableUrl()
{
    QSignalSpy spyError(m_client, SIGNAL(errorOccurred(int,QString)));
    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));

    // Порт 1 заведомо закрыт
    QUrl badUrl("ws://127.0.0.1:1");
    m_client->connectToUrl(badUrl);

    // При ошибке подключения WebSocket тоже переходит в Disconnected
    QVERIFY(spyError.wait(5000) || spyDisconnected.wait(3000));
    QCOMPARE(m_client->state(), WsClient::State::Disconnected);
    QVERIFY(spyError.count() > 0);
}
// ---------------------------------------------------------------------------
// Точка входа
// ---------------------------------------------------------------------------
QTEST_MAIN(WsClientTest)
#include "tst_wsclient.moc"