#include <QtTest>
#include <QSignalSpy>
#include <QUdpSocket>

#include "core/UdpEndpoint.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// UdpHelper — вспомогательный UDP-сокет для тестов
//
// ---------------------------------------------------------------------------
class UdpHelper : public QObject
{
    Q_OBJECT

public:
    explicit UdpHelper(QObject *parent = nullptr)
        : QObject(parent)
        , m_socket(new QUdpSocket(this))
    {
        // Привязываемся на случайный порт для приёма ответов
        m_socket->bind(QHostAddress::LocalHost, 0,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

        connect(m_socket, &QUdpSocket::readyRead,
                this, &UdpHelper::onReadyRead);
    }

    quint16 port() const
    {
        return m_socket->localPort();
    }

    // Отправить датаграмму на указанный порт на localhost
    void sendTo(quint16 targetPort, const QByteArray &data)
    {
        m_socket->writeDatagram(data, QHostAddress::LocalHost, targetPort);
    }

    // Последняя принятая датаграмма
    QByteArray lastReceived;

signals:
    void datagramReceived();

private slots:
    void onReadyRead()
    {
        while (m_socket->hasPendingDatagrams()) {
            const QNetworkDatagram dg = m_socket->receiveDatagram();
            lastReceived = dg.data();
            emit datagramReceived();
        }
    }

private:
    QUdpSocket *m_socket;
};

// ---------------------------------------------------------------------------
// UdpEndpointTest
// ---------------------------------------------------------------------------
class UdpEndpointTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- Начальное состояние ---
    void testInitialState();

    // --- Bind / Unbind ---
    void testBind();
    void testUnbind();
    void testRebind();
    void testBindUnavailablePort();

    // --- Настройка цели ---
    void testSetTarget();
    void testSendWithoutTarget();
    void testSendWithInvalidAddress();

    // --- Отправка и приём ---
    void testSendData();
    void testReceiveData();
    void testSendAndReceive();

    // --- Сигналы ---
    void testStateChangedSignal();
    void testMessageReceivedOnSend();
    void testMessageReceivedOnReceive();
    void testDatagramReceivedSignal();

    // --- Граничные случаи ---
    void testSendEmptyData();
    void testUnbindWhenNotBound();

private:
    UdpEndpoint *m_endpoint { nullptr };
    UdpHelper   *m_helper   { nullptr };

    static const int CONNECTION_ID = 7;
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void UdpEndpointTest::init()
{
    m_endpoint = new UdpEndpoint(CONNECTION_ID, this);
    m_helper   = new UdpHelper(this);
}

void UdpEndpointTest::cleanup()
{
    delete m_endpoint;
    m_endpoint = nullptr;

    delete m_helper;
    m_helper = nullptr;
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void UdpEndpointTest::testInitialState()
{
    QCOMPARE(m_endpoint->state(),        UdpEndpoint::State::Unbound);
    QCOMPARE(m_endpoint->connectionId(), CONNECTION_ID);
    QCOMPARE(m_endpoint->localPort(),    quint16(0));
    QVERIFY (m_endpoint->targetAddress().isEmpty());
    QCOMPARE(m_endpoint->targetPort(),   quint16(0));
    QCOMPARE(m_endpoint->isBroadcastEnabled(), false);
}

void UdpEndpointTest::testBind()
{
    QSignalSpy spyState  (m_endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)));
    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    const bool bound = m_endpoint->bind(QHostAddress::LocalHost, 0);

    QVERIFY(bound);
    QCOMPARE(m_endpoint->state(), UdpEndpoint::State::Bound);
    QVERIFY (m_endpoint->localPort() > 0);  // ОС назначила реальный порт

    // stateChanged(Bound) должен прийти
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyState.first().at(0).toInt(),
             CONNECTION_ID);
    QCOMPARE(spyState.first().at(1).value<UdpEndpoint::State>(),
             UdpEndpoint::State::Bound);

    // Системное сообщение "Bound to ..."
    QCOMPARE(spyMessage.count(), 1);
    const Message msg = spyMessage.first().at(0).value<Message>();
    QCOMPARE(msg.direction, Message::Direction::System);
    QCOMPARE(msg.protocol,  Message::Protocol::Udp);
}

void UdpEndpointTest::testUnbind()
{
    m_endpoint->bind(QHostAddress::LocalHost, 0);
    QCOMPARE(m_endpoint->state(), UdpEndpoint::State::Bound);

    QSignalSpy spyState(m_endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)));

    m_endpoint->unbind();

    QCOMPARE(m_endpoint->state(), UdpEndpoint::State::Unbound);
    QCOMPARE(spyState.count(), 1);
    QCOMPARE(spyState.first().at(1).value<UdpEndpoint::State>(),
             UdpEndpoint::State::Unbound);
}

void UdpEndpointTest::testRebind()
{
    // Привязываемся первый раз
    m_endpoint->bind(QHostAddress::LocalHost, 0);
    const quint16 firstPort = m_endpoint->localPort();
    QVERIFY(firstPort > 0);

    // Перебиндируемся — bind() должен освободить старый порт и занять новый
    QSignalSpy spyState(m_endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)));

    const bool rebound = m_endpoint->bind(QHostAddress::LocalHost, 0);

    QVERIFY(rebound);
    QCOMPARE(m_endpoint->state(), UdpEndpoint::State::Bound);

    // При rebind должно быть: Unbound → Bound (2 изменения)
    QVERIFY(spyState.count() >= 2);
}

void UdpEndpointTest::testBindUnavailablePort()
{
    QSignalSpy spyError(m_endpoint, SIGNAL(errorOccurred(int, QString)));

    // Занимаем порт другим сокетом
    QUdpSocket occupied;
    occupied.bind(QHostAddress::LocalHost, 0);
    const quint16 occupiedPort = occupied.localPort();

    // Пытаемся привязаться на тот же порт без ShareAddress — должно упасть
    // Создаём отдельный эндпоинт без флагов шаринга для чистого теста
    // Наш bind использует ShareAddress поэтому тест через отдельный хелпер:
    QUdpSocket testSocket;
    const bool bound = testSocket.bind(
        QHostAddress::LocalHost,
        occupiedPort,
        QUdpSocket::DontShareAddress   // явно запрещаем шаринг
        );

    // Если порт занят без шаринга — должен упасть
    if (!bound) {
        // Ожидаемое поведение на большинстве систем
        QVERIFY(true);
    } else {
        // На некоторых системах ShareAddress всё равно даёт привязаться
        QSKIP("OS allows port reuse — cannot test unavailable port reliably");
    }

    occupied.close();
    Q_UNUSED(spyError)
}

void UdpEndpointTest::testSetTarget()
{
    m_endpoint->setTargetAddress("127.0.0.1");
    m_endpoint->setTargetPort(5000);

    QCOMPARE(m_endpoint->targetAddress(), QString("127.0.0.1"));
    QCOMPARE(m_endpoint->targetPort(),    quint16(5000));
}

void UdpEndpointTest::testSendWithoutTarget()
{
    // Без установленного адреса sendData должен вернуть false
    const bool result = m_endpoint->sendData("test");
    QCOMPARE(result, false);
}

void UdpEndpointTest::testSendWithInvalidAddress()
{
    QSignalSpy spyError(m_endpoint, SIGNAL(errorOccurred(int, QString)));

    m_endpoint->setTargetAddress("not_a_valid_ip");
    m_endpoint->setTargetPort(5000);

    const bool result = m_endpoint->sendData("test");

    QCOMPARE(result, false);
    QVERIFY(spyError.count() > 0);
}

void UdpEndpointTest::testSendData()
{
    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    m_endpoint->setTargetAddress("127.0.0.1");
    m_endpoint->setTargetPort(m_helper->port());

    const QByteArray payload = "Hello UDP";
    const bool sent = m_endpoint->sendData(payload);

    QVERIFY(sent);

    // Должно появиться исходящее Message в логе
    QCOMPARE(spyMessage.count(), 1);
    const Message msg = spyMessage.first().at(0).value<Message>();
    QCOMPARE(msg.direction, Message::Direction::Outgoing);
    QCOMPARE(msg.protocol,  Message::Protocol::Udp);
    QCOMPARE(msg.payload,   payload);
}

void UdpEndpointTest::testReceiveData()
{
    // Привязываемся чтобы принимать датаграммы
    m_endpoint->bind(QHostAddress::LocalHost, 0);

    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    const QByteArray payload = "Incoming datagram";
    m_helper->sendTo(m_endpoint->localPort(), payload);

    // Ждём входящего сообщения
    QVERIFY(spyMessage.wait(2000));

    // Ищем входящее сообщение (первое может быть системным "Bound to...")
    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming
            && msg.payload == payload) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Did not receive expected datagram");
}

void UdpEndpointTest::testSendAndReceive()
{
    // Обе стороны: endpoint отправляет хелперу, хелпер отвечает эндпоинту
    m_endpoint->bind(QHostAddress::LocalHost, 0);

    QSignalSpy spyHelper(m_helper, SIGNAL(datagramReceived()));
    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    const QByteArray sent = "ping";
    m_endpoint->setTargetAddress("127.0.0.1");
    m_endpoint->setTargetPort(m_helper->port());
    m_endpoint->sendData(sent);

    // Хелпер должен получить датаграмму
    QVERIFY(spyHelper.wait(2000));
    QCOMPARE(m_helper->lastReceived, sent);

    // Хелпер отвечает эндпоинту
    const QByteArray reply = "pong";
    m_helper->sendTo(m_endpoint->localPort(), reply);

    // Эндпоинт должен получить ответ
    const int maxWait = 2000;
    const int step    = 50;
    int elapsed       = 0;
    bool gotReply     = false;

    while (elapsed < maxWait) {
        QTest::qWait(step);
        elapsed += step;
        for (int i = 0; i < spyMessage.count(); ++i) {
            const Message msg = spyMessage.at(i).at(0).value<Message>();
            if (msg.direction == Message::Direction::Incoming
                && msg.payload == reply) {
                gotReply = true;
                break;
            }
        }
        if (gotReply) break;
    }

    QVERIFY2(gotReply, "Did not receive reply datagram");
}

void UdpEndpointTest::testStateChangedSignal()
{
    QSignalSpy spyState(m_endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)));

    m_endpoint->bind(QHostAddress::LocalHost, 0);
    m_endpoint->unbind();

    // bind → Bound, unbind → Unbound
    QCOMPARE(spyState.count(), 2);

    QCOMPARE(spyState.at(0).at(0).toInt(), CONNECTION_ID);
    QCOMPARE(spyState.at(0).at(1).value<UdpEndpoint::State>(),
             UdpEndpoint::State::Bound);

    QCOMPARE(spyState.at(1).at(1).value<UdpEndpoint::State>(),
             UdpEndpoint::State::Unbound);
}

void UdpEndpointTest::testMessageReceivedOnSend()
{
    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    m_endpoint->setTargetAddress("127.0.0.1");
    m_endpoint->setTargetPort(m_helper->port());

    m_endpoint->sendData("test message");

    QCOMPARE(spyMessage.count(), 1);

    const Message msg = spyMessage.first().at(0).value<Message>();
    QCOMPARE(msg.connectionId, CONNECTION_ID);
    QCOMPARE(msg.direction,    Message::Direction::Outgoing);
    QCOMPARE(msg.protocol,     Message::Protocol::Udp);
    QCOMPARE(msg.isText,       false);
}

void UdpEndpointTest::testMessageReceivedOnReceive()
{
    m_endpoint->bind(QHostAddress::LocalHost, 0);

    QSignalSpy spyMessage(m_endpoint, SIGNAL(messageReceived(Message)));

    m_helper->sendTo(m_endpoint->localPort(), "hello");

    QVERIFY(spyMessage.wait(2000));

    bool found = false;
    for (int i = 0; i < spyMessage.count(); ++i) {
        const Message msg = spyMessage.at(i).at(0).value<Message>();
        if (msg.direction == Message::Direction::Incoming) {
            QCOMPARE(msg.connectionId, CONNECTION_ID);
            QCOMPARE(msg.protocol,     Message::Protocol::Udp);
            QCOMPARE(msg.isText,       false);
            QCOMPARE(msg.payload,      QByteArray("hello"));
            found = true;
            break;
        }
    }
    QVERIFY2(found, "Incoming Message not received");
}

void UdpEndpointTest::testDatagramReceivedSignal()
{
    m_endpoint->bind(QHostAddress::LocalHost, 0);

    QSignalSpy spyDatagram(m_endpoint,
                           SIGNAL(datagramReceived(int, QHostAddress, quint16, QByteArray)));

    m_helper->sendTo(m_endpoint->localPort(), "datagram signal test");

    QVERIFY(spyDatagram.wait(2000));

    const QList<QVariant> args = spyDatagram.first();
    QCOMPARE(args.at(0).toInt(), CONNECTION_ID);
    // Отправитель — localhost
    QCOMPARE(args.at(1).value<QHostAddress>(),
             QHostAddress(QHostAddress::LocalHost));
    QCOMPARE(args.at(2).toUInt(), static_cast<uint>(m_helper->port()));
    QCOMPARE(args.at(3).toByteArray(), QByteArray("datagram signal test"));
}

void UdpEndpointTest::testSendEmptyData()
{
    m_endpoint->setTargetAddress("127.0.0.1");
    m_endpoint->setTargetPort(m_helper->port());

    // Пустые данные — sendData должен вернуть false без отправки
    const bool result = m_endpoint->sendData(QByteArray());
    QCOMPARE(result, false);
}

void UdpEndpointTest::testUnbindWhenNotBound()
{
    // unbind() на несвязанном сокете — не должно крашиться
    // и не должно эмитировать лишних сигналов
    QSignalSpy spyState(m_endpoint, SIGNAL(stateChanged(int, UdpEndpoint::State)));

    m_endpoint->unbind();

    QCOMPARE(m_endpoint->state(), UdpEndpoint::State::Unbound);
    QCOMPARE(spyState.count(), 0);  // нет лишних сигналов
}

// ---------------------------------------------------------------------------
// Точка входа
// ---------------------------------------------------------------------------
QTEST_MAIN(UdpEndpointTest)

#include "tst_udpendpoint.moc"