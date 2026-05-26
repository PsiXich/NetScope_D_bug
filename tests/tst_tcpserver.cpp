#include <QTest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QHostAddress>

#include "core/TcpServer.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// TcpServerTest
// ---------------------------------------------------------------------------
class TcpServerTest : public QObject
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
    void testStartStopListening();
    void testClientConnection();
    void testReceiveData();
    void testSendToClient();
    void testBroadcast();
    void testClientDisconnection();

private:
    TcpServer *m_server { nullptr };
    const int CONNECTION_ID = 42;
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void TcpServerTest::init()
{
    m_server = new TcpServer(CONNECTION_ID, this);
}

void TcpServerTest::cleanup()
{
    if (m_server) {
        m_server->stopListening();
        delete m_server;
        m_server = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void TcpServerTest::testInitialState()
{
    QCOMPARE(m_server->connectionId(), CONNECTION_ID);
    QCOMPARE(m_server->isListening(), false);
    QCOMPARE(m_server->clientCount(), 0);
    QVERIFY(m_server->sessions().isEmpty());
}

void TcpServerTest::testStartStopListening()
{
    QSignalSpy spyListening(m_server, SIGNAL(listeningChanged(bool)));

    // Запускаем сервер на случайном свободном порту
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));
    QCOMPARE(m_server->isListening(), true);
    QVERIFY(m_server->port() > 0);

    QCOMPARE(spyListening.count(), 1);
    QCOMPARE(spyListening.first().at(0).toBool(), true);

    // Останавливаем
    m_server->stopListening();
    QCOMPARE(m_server->isListening(), false);

    QCOMPARE(spyListening.count(), 2);
    QCOMPARE(spyListening.last().at(0).toBool(), false);
}

void TcpServerTest::testClientConnection()
{
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));

    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(qintptr,QString)));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(qintptr,QString)));

    // Создаем клиента и подключаемся к серверу
    QTcpSocket client;
    client.connectToHost(m_server->address(), m_server->port());

    // Ждем пока сервер обработает подключение
    QVERIFY(spyConnected.wait(2000));

    QCOMPARE(spyConnected.count(), 1);
    QCOMPARE(m_server->clientCount(), 1);

    const qintptr descriptor = spyConnected.first().at(0).toLongLong();
    QVERIFY(descriptor > 0);

    // Проверяем, что сессия добавлена
    const auto sessions = m_server->sessions();
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions.first().descriptor, descriptor);

    // Опрятно закрываем сессию до завершения метода, чтобы избежать QWARN в рантайме
    // Далее в остальных тестах также
    client.disconnectFromHost();
    QVERIFY(spyDisconnected.wait(2000));
}

void TcpServerTest::testReceiveData()
{
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));

    QSignalSpy spyMessage(m_server, SIGNAL(messageReceived(Message)));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(qintptr,QString)));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(qintptr,QString)));

    QTcpSocket client;
    client.connectToHost(m_server->address(), m_server->port());
    QVERIFY(spyConnected.wait(2000));
    QVERIFY(client.waitForConnected(2000));

    // Очищаем от системного сообщения о подключении клиента
    spyMessage.clear();

    // Клиент отправляет данные
    const QByteArray testData = "Hello from Client";
    client.write(testData);
    QVERIFY(client.waitForBytesWritten(2000));

    // Ждем сообщение с данными
    QVERIFY(spyMessage.wait(2000));

    QCOMPARE(spyMessage.count(), 1);
    // Проверяем именно то сообщение, которое содержит данные (Incoming)
    const Message msg = spyMessage.first().at(0).value<Message>();

    QCOMPARE(msg.connectionId, CONNECTION_ID);
    QCOMPARE(msg.protocol, Message::Protocol::TcpServer);
    QCOMPARE(msg.direction, Message::Direction::Incoming);
    QCOMPARE(msg.payload, testData);

    client.disconnectFromHost();
    QVERIFY(spyDisconnected.wait(2000));
}

void TcpServerTest::testSendToClient()
{
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(qintptr,QString)));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(qintptr,QString)));

    QTcpSocket client;
    client.connectToHost(m_server->address(), m_server->port());
    QVERIFY(spyConnected.wait(2000));

    const qintptr descriptor = spyConnected.first().at(0).toLongLong();

    QSignalSpy clientReadyRead(&client, SIGNAL(readyRead()));

    // Сервер отправляет данные конкретному клиенту
    const QByteArray testData = "Hello from Server";
    QVERIFY(m_server->sendToClient(descriptor, testData));

    // Ждем, пока клиент получит данные
    QVERIFY(clientReadyRead.wait(2000));
    QCOMPARE(client.readAll(), testData);

    client.disconnectFromHost();
    QVERIFY(spyDisconnected.wait(2000));
}

void TcpServerTest::testBroadcast()
{
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(qintptr,QString)));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(qintptr,QString)));

    // Подключаем двух клиентов
    QTcpSocket client1, client2;
    client1.connectToHost(m_server->address(), m_server->port());
    client2.connectToHost(m_server->address(), m_server->port());

    // Ждем подключения обоих
    while (spyConnected.count() < 2) {
        QVERIFY(spyConnected.wait(2000));
    }

    QCOMPARE(m_server->clientCount(), 2);

    QSignalSpy client1ReadyRead(&client1, SIGNAL(readyRead()));
    QSignalSpy client2ReadyRead(&client2, SIGNAL(readyRead()));

    // Сервер делает бродкаст
    const QByteArray testData = "Broadcast Message";
    m_server->broadcast(testData);

    // Оба клиента должны получить данные
    QVERIFY(client1ReadyRead.wait(2000) || client1.bytesAvailable() > 0);
    QVERIFY(client2ReadyRead.wait(2000) || client2.bytesAvailable() > 0);

    QCOMPARE(client1.readAll(), testData);
    QCOMPARE(client2.readAll(), testData);

    client1.disconnectFromHost();
    client2.disconnectFromHost();
    while (spyDisconnected.count() < 2) {
        QVERIFY(spyDisconnected.wait(2000));
    }
}

void TcpServerTest::testClientDisconnection()
{
    QVERIFY(m_server->startListening(QHostAddress::LocalHost, 0));
    QSignalSpy spyConnected(m_server, SIGNAL(clientConnected(qintptr,QString)));
    QSignalSpy spyDisconnected(m_server, SIGNAL(clientDisconnected(qintptr,QString)));

    QTcpSocket *client = new QTcpSocket(this);
    client->connectToHost(m_server->address(), m_server->port());
    QVERIFY(spyConnected.wait(2000));

    QCOMPARE(m_server->clientCount(), 1);

    // Разрываем соединение со стороны клиента
    client->disconnectFromHost();

    // Ждем, пока сервер обработает отключение
    QVERIFY(spyDisconnected.wait(2000));

    QCOMPARE(spyDisconnected.count(), 1);
    QCOMPARE(m_server->clientCount(), 0);
    QVERIFY(m_server->sessions().isEmpty());

    client->deleteLater();
}
// ---------------------------------------------------------------------------
// Точка входа QTest
// ---------------------------------------------------------------------------
QTEST_MAIN(TcpServerTest)
#include "tst_tcpserver.moc"