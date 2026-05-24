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
        for (QTcpSocket *client : m_clients) {
            client->disconnectFromHost();
        }
    }

private slots:
    void onNewConnection()
    {
        while (m_server->hasPendingConnections()) {
            QTcpSocket *client = m_server->nextPendingConnection();
            m_clients.append(client);
            ++m_connectionsCount;

            connect(client, SIGNAL(readyRead()),
                    this, SLOT(onReadyRead()));
            connect(client, SIGNAL(disconnected()),
                    client, SLOT(deleteLater()));
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
    QVERIFY(spyDisconnected.wait(2000));

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
    QVERIFY(spyConnected.wait(2000));

    m_client->disconnectFromHost();
    QVERIFY(spyDisconnected.wait(2000));

    // Проверяем что connectionId в сигналах совпадает
    for (int i = 0; i < spyState.count(); ++i) {
        const int id = spyState.at(i).at(0).toInt();
        QCOMPARE(id, CONNECTION_ID);
    }

    // Последнее состояние должно быть Disconnected
    const TcpClient::State lastState =
        spyState.last().at(1).value<TcpClient::State>();
    QCOMPARE(lastState, TcpClient::State::Disconnected);
}

void TcpClientTest::testReconnect()
{
    // Устанавливаем интервал реконнекта 300 мс
    m_client->setReconnectInterval(300);
    QCOMPARE(m_client->reconnectInterval(), 300);

    QSignalSpy spyConnected(m_client, SIGNAL(connected(int)));

    m_client->connectToHost("127.0.0.1", m_server->port());
    QVERIFY(spyConnected.wait(2000));

    // Сервер принудительно закрывает соединение
    m_server->disconnectAll();

    QSignalSpy spyDisconnected(m_client, SIGNAL(disconnected(int)));
    QVERIFY(spyDisconnected.wait(2000));

    // Ждём автоматического переподключения
    // Таймаут = интервал реконнекта + запас на установку соединения
    QVERIFY(spyConnected.wait(1500));
    QCOMPARE(m_client->state(), TcpClient::State::Connected);
}

void TcpClientTest::testConnectToUnavailableHost()
{
    QSignalSpy spyError(m_client, SIGNAL(errorOccurred(int, QString)));

    // Порт 1 — зарезервирован, соединение будет отклонено
    m_client->connectToHost("127.0.0.1", 1);

    // Ждём сигнал ошибки
    QVERIFY(spyError.wait(5000));

    const int    id  = spyError.first().at(0).toInt();
    const QString err = spyError.first().at(1).toString();

    QCOMPARE(id, CONNECTION_ID);
    QVERIFY(!err.isEmpty());
    QCOMPARE(m_client->state(), TcpClient::State::Disconnected);
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

// Включаем MOC-файл сгенерированный AUTOMOC —
// обязательно в конце .cpp файла теста
#include "tst_tcpclient.moc"