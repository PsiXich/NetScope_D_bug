#include <QtTest>

#include "models/messagelogmodel.h"
#include "core/Message.h"

// ---------------------------------------------------------------------------
// Вспомогательные фабрики — создают типовые сообщения для тестов
// Вынесены как свободные функции чтобы не дублировать в каждом тесте
// ---------------------------------------------------------------------------
static Message makeTcpIncoming(int connId, const QByteArray &data)
{
    return Message::incoming(connId, Message::Protocol::Tcp, data, false);
}

static Message makeTcpOutgoing(int connId, const QByteArray &data)
{
    return Message::outgoing(connId, Message::Protocol::Tcp, data, false);
}

static Message makeWsText(int connId, const QString &text)
{
    return Message::incoming(connId,
                             Message::Protocol::WebSocket,
                             text.toUtf8(),
                             true);
}

static Message makeSystem(int connId)
{
    return Message::system(connId, Message::Protocol::Tcp, "Connected");
}

// ---------------------------------------------------------------------------
// MessageLogModelTest
// ---------------------------------------------------------------------------
class MessageLogModelTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // --- Базовое поведение ---
    void testInitialState();
    void testAppendMessage();
    void testClear();
    void testRowCountMatchesFiltered();

    // --- Данные модели ---
    void testDataDisplayRole();
    void testDataRawMessageRole();
    void testDataDirectionRole();
    void testDataIsTextRole();
    void testHeaderData();

    // --- Фильтрация по connectionId ---
    void testFilterByConnectionId();
    void testFilterConnectionIdShowAll();
    void testFilterConnectionIdNoMatch();

    // --- Фильтрация по протоколу ---
    void testFilterByProtocol();
    void testFilterProtocolShowAll();

    // --- Фильтрация по направлению ---
    void testFilterByDirection();
    void testClearFilters();

    // --- Фильтрация по тексту ---
    void testFilterByText();
    void testFilterByTextCaseInsensitive();

    // --- Комбинированные фильтры ---
    void testCombinedFilters();

    // --- Ограничение строк ---
    void testMaxRowCount();
    void testMaxRowCountZeroMeansUnlimited();

    // --- Граничные случаи ---
    void testAppendEmptyPayload();
    void testSystemMessageHasNoSize();

private:
    MessageLogModel *m_model { nullptr };

    // Добавить N сообщений в модель для connId
    void fillModel(int count, int connId = 0);
};

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

void MessageLogModelTest::init()
{
    m_model = new MessageLogModel(this);
}

void MessageLogModelTest::cleanup()
{
    delete m_model;
    m_model = nullptr;
}

// ---------------------------------------------------------------------------
// Вспомогательный метод
// ---------------------------------------------------------------------------

void MessageLogModelTest::fillModel(int count, int connId)
{
    for (int i = 0; i < count; ++i) {
        m_model->appendMessage(
            makeTcpIncoming(connId, QByteArray("msg") + QByteArray::number(i))
            );
    }
}

// ---------------------------------------------------------------------------
// Тесты
// ---------------------------------------------------------------------------

void MessageLogModelTest::testInitialState()
{
    QCOMPARE(m_model->rowCount(), 0);
    QCOMPARE(m_model->columnCount(), static_cast<int>(MessageLogModel::ColCount));
    QCOMPARE(m_model->maxRowCount(), 10000);
    QCOMPARE(m_model->filterConnectionId(), -1);
    QCOMPARE(m_model->filterDirection(), -1);
    QCOMPARE(m_model->filterProtocol(), Message::Protocol::Unknown);
}

void MessageLogModelTest::testAppendMessage()
{
    QSignalSpy spy(m_model, SIGNAL(rowsInserted(QModelIndex, int, int)));

    m_model->appendMessage(makeTcpIncoming(0, "hello"));

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);

    // Проверяем диапазон вставки
    const QList<QVariant> args = spy.first();
    QCOMPARE(args.at(1).toInt(), 0);    // first
    QCOMPARE(args.at(2).toInt(), 0);    // last
}

void MessageLogModelTest::testClear()
{
    fillModel(5);
    QCOMPARE(m_model->rowCount(), 5);

    QSignalSpy spy(m_model, SIGNAL(modelReset()));
    m_model->clear();

    QCOMPARE(m_model->rowCount(), 0);
    QCOMPARE(spy.count(), 1);
}

void MessageLogModelTest::testRowCountMatchesFiltered()
{
    // Добавляем сообщения для двух соединений
    m_model->appendMessage(makeTcpIncoming(0, "a"));
    m_model->appendMessage(makeTcpIncoming(1, "b"));
    m_model->appendMessage(makeTcpIncoming(0, "c"));

    QCOMPARE(m_model->rowCount(), 3);   // без фильтра — все три

    m_model->setFilterConnectionId(0);
    QCOMPARE(m_model->rowCount(), 2);   // только connId=0

    m_model->setFilterConnectionId(1);
    QCOMPARE(m_model->rowCount(), 1);   // только connId=1
}

void MessageLogModelTest::testDataDisplayRole()
{
    const QByteArray payload = "Hello";

    // явно указывая isText = true,
    // чтобы форматтер модели не превратил строку в Hex
    const Message msg = Message::incoming(42, Message::Protocol::Tcp, payload, true);
    m_model->appendMessage(msg);

    const QModelIndex idx = m_model->index(0, MessageLogModel::ColId);
    QCOMPARE(idx.data(Qt::DisplayRole).toInt(), 42);

    const QModelIndex dataIdx = m_model->index(0, MessageLogModel::ColData);
    QVERIFY(dataIdx.data(Qt::DisplayRole).toString().contains("Hello"));

    const QModelIndex protoIdx = m_model->index(0, MessageLogModel::ColProtocol);
    QCOMPARE(protoIdx.data(Qt::DisplayRole).toString(), QString("TCP"));

    const QModelIndex dirIdx = m_model->index(0, MessageLogModel::ColDirection);
    QVERIFY(dirIdx.data(Qt::DisplayRole).toString().contains("In"));
}

void MessageLogModelTest::testDataRawMessageRole()
{
    const Message original = makeTcpIncoming(7, "raw_test");
    m_model->appendMessage(original);

    const QModelIndex idx = m_model->index(0, 0);
    const Message retrieved = idx.data(MessageLogModel::RawMessageRole)
                                  .value<Message>();

    QCOMPARE(retrieved.connectionId, original.connectionId);
    QCOMPARE(retrieved.payload,      original.payload);
    QCOMPARE(retrieved.protocol,     original.protocol);
    QCOMPARE(retrieved.direction,    original.direction);
}

void MessageLogModelTest::testDataDirectionRole()
{
    m_model->appendMessage(makeTcpIncoming(0, "x"));
    m_model->appendMessage(makeTcpOutgoing(0, "y"));
    m_model->appendMessage(makeSystem(0));

    const auto dirAt = [this](int row) {
        return m_model->index(row, 0)
        .data(MessageLogModel::DirectionRole)
            .value<Message::Direction>();
    };

    QCOMPARE(dirAt(0), Message::Direction::Incoming);
    QCOMPARE(dirAt(1), Message::Direction::Outgoing);
    QCOMPARE(dirAt(2), Message::Direction::System);
}

void MessageLogModelTest::testDataIsTextRole()
{
    m_model->appendMessage(makeTcpIncoming(0, "binary"));    // isText = false
    m_model->appendMessage(makeWsText(0, "text message"));   // isText = true

    QCOMPARE(m_model->index(0, 0).data(MessageLogModel::IsTextRole).toBool(),
             false);
    QCOMPARE(m_model->index(1, 0).data(MessageLogModel::IsTextRole).toBool(),
             true);
}

void MessageLogModelTest::testHeaderData()
{
    // Горизонтальные заголовки должны быть непустыми строками
    for (int col = 0; col < MessageLogModel::ColCount; ++col) {
        const QVariant header = m_model->headerData(
            col, Qt::Horizontal, Qt::DisplayRole
            );
        QVERIFY(!header.toString().isEmpty());
    }

    // Вертикальные заголовки не используются — возвращают QVariant()
    const QVariant vHeader = m_model->headerData(
        0, Qt::Vertical, Qt::DisplayRole
        );
    QVERIFY(!vHeader.isValid());
}

void MessageLogModelTest::testFilterByConnectionId()
{
    m_model->appendMessage(makeTcpIncoming(0, "conn0_msg1"));
    m_model->appendMessage(makeTcpIncoming(1, "conn1_msg1"));
    m_model->appendMessage(makeTcpIncoming(0, "conn0_msg2"));
    m_model->appendMessage(makeTcpIncoming(2, "conn2_msg1"));

    QSignalSpy spy(m_model, SIGNAL(modelReset()));
    m_model->setFilterConnectionId(0);

    // Смена фильтра должна сбросить модель
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->rowCount(), 2);

    // Проверяем что показываются только сообщения connId=0
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const Message msg = m_model->messageAt(row);
        QCOMPARE(msg.connectionId, 0);
    }
}

void MessageLogModelTest::testFilterConnectionIdShowAll()
{
    fillModel(3, 0);
    fillModel(2, 1);

    m_model->setFilterConnectionId(0);
    QCOMPARE(m_model->rowCount(), 3);

    // -1 = показать все
    m_model->setFilterConnectionId(-1);
    QCOMPARE(m_model->rowCount(), 5);
}

void MessageLogModelTest::testFilterConnectionIdNoMatch()
{
    fillModel(3, 0);

    // Фильтр по несуществующему id — ноль строк
    m_model->setFilterConnectionId(999);
    QCOMPARE(m_model->rowCount(), 0);
}

void MessageLogModelTest::testFilterByProtocol()
{
    m_model->appendMessage(
        Message::incoming(0, Message::Protocol::Tcp, "tcp", false));
    m_model->appendMessage(
        Message::incoming(0, Message::Protocol::WebSocket, "ws", true));
    m_model->appendMessage(
        Message::incoming(0, Message::Protocol::TcpServer, "svr", false));

    m_model->setFilterProtocol(Message::Protocol::Tcp);
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).protocol, Message::Protocol::Tcp);

    m_model->setFilterProtocol(Message::Protocol::WebSocket);
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).protocol, Message::Protocol::WebSocket);
}

void MessageLogModelTest::testFilterProtocolShowAll()
{
    m_model->appendMessage(
        Message::incoming(0, Message::Protocol::Tcp, "tcp", false));
    m_model->appendMessage(
        Message::incoming(0, Message::Protocol::WebSocket, "ws", true));

    m_model->setFilterProtocol(Message::Protocol::Tcp);
    QCOMPARE(m_model->rowCount(), 1);

    // Unknown = показать все протоколы
    m_model->setFilterProtocol(Message::Protocol::Unknown);
    QCOMPARE(m_model->rowCount(), 2);
}

void MessageLogModelTest::testFilterByDirection()
{
    m_model->appendMessage(makeTcpIncoming(0, "in"));
    m_model->appendMessage(makeTcpOutgoing(0, "out"));
    m_model->appendMessage(makeSystem(0));

    m_model->setFilterDirection(
        static_cast<int>(Message::Direction::Incoming));
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).direction, Message::Direction::Incoming);

    m_model->setFilterDirection(
        static_cast<int>(Message::Direction::Outgoing));
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).direction, Message::Direction::Outgoing);

    m_model->setFilterDirection(
        static_cast<int>(Message::Direction::System));
    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).direction, Message::Direction::System);
}

void MessageLogModelTest::testClearFilters()
{
    fillModel(3, 0);
    fillModel(2, 1);

    m_model->setFilterConnectionId(0);
    QCOMPARE(m_model->rowCount(), 3);

    m_model->clearFilters();
    QCOMPARE(m_model->rowCount(), 5);
    QCOMPARE(m_model->filterConnectionId(), -1);
    QCOMPARE(m_model->filterDirection(), -1);
    QCOMPARE(m_model->filterProtocol(), Message::Protocol::Unknown);
}

void MessageLogModelTest::testCombinedFilters()
{
    // connId=0, TCP, Incoming
    m_model->appendMessage(makeTcpIncoming(0, "a"));
    // connId=0, TCP, Outgoing
    m_model->appendMessage(makeTcpOutgoing(0, "b"));
    // connId=1, TCP, Incoming
    m_model->appendMessage(makeTcpIncoming(1, "c"));
    // connId=0, WS, Incoming
    m_model->appendMessage(makeWsText(0, "d"));

    // Фильтр: connId=0 + TCP + Incoming → только "a"
    m_model->setFilterConnectionId(0);
    m_model->setFilterProtocol(Message::Protocol::Tcp);
    m_model->setFilterDirection(
        static_cast<int>(Message::Direction::Incoming));

    QCOMPARE(m_model->rowCount(), 1);
    QCOMPARE(m_model->messageAt(0).payload, QByteArray("a"));
}

void MessageLogModelTest::testMaxRowCount()
{
    m_model->setMaxRowCount(5);
    QCOMPARE(m_model->maxRowCount(), 5);

    // Добавляем 8 сообщений — должно остаться 5
    fillModel(8);
    QVERIFY(m_model->rowCount() <= 5);
}

void MessageLogModelTest::testMaxRowCountZeroMeansUnlimited()
{
    m_model->setMaxRowCount(0);
    fillModel(100);
    QCOMPARE(m_model->rowCount(), 100);
}

void MessageLogModelTest::testAppendEmptyPayload()
{
    // Системное сообщение с пустым payload — допустимо
    const Message sys = Message::system(0, Message::Protocol::Tcp, "test info");
    m_model->appendMessage(sys);
    QCOMPARE(m_model->rowCount(), 1);

    // ColSize для System — пустой QVariant
    const QModelIndex sizeIdx = m_model->index(0, MessageLogModel::ColSize);
    QVERIFY(!sizeIdx.data(Qt::DisplayRole).isValid());
}

void MessageLogModelTest::testSystemMessageHasNoSize()
{
    m_model->appendMessage(makeSystem(0));

    const QModelIndex idx = m_model->index(0, MessageLogModel::ColSize);
    // Системные сообщения не имеют размера — возвращаем invalid QVariant
    QVERIFY(!idx.data(Qt::DisplayRole).isValid());
}

void MessageLogModelTest::testFilterByText()
{
    // 2 подходят под фильтр "error", 2 — нет
    m_model->appendMessage(makeWsText(0, "Connection timeout error")); // Подходит (текст)
    m_model->appendMessage(makeWsText(0, "User logged in"));           // Мимо
    m_model->appendMessage(makeTcpIncoming(0, "fatal_error_code"));    // Подходит (бинарное, но содержит байты "error")
    m_model->appendMessage(makeTcpIncoming(1, "ping"));                // Мимо

    QSignalSpy spy(m_model, SIGNAL(modelReset()));

    m_model->setFilterText("error");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->rowCount(), 2);

    // Убеждаемся, что остались правильные сообщения
    QVERIFY(m_model->messageAt(0).payload.contains("timeout"));
    QVERIFY(m_model->messageAt(1).payload.contains("fatal"));
}

void MessageLogModelTest::testFilterByTextCaseInsensitive()
{
    m_model->appendMessage(makeWsText(0, "WARNING: CPU overload"));
    m_model->appendMessage(makeWsText(0, "Everything is fine"));

    // Фильтр в нижнем регистре, а текст в логе — в верхнем
    m_model->setFilterText("warning");

    // Для текстовых сообщений (isText = true) поиск должен игнорировать регистр
    QCOMPARE(m_model->rowCount(), 1);
    QVERIFY(QString::fromUtf8(m_model->messageAt(0).payload).startsWith("WARNING"));
}

// ---------------------------------------------------------------------------
// Точка входа
// ---------------------------------------------------------------------------
QTEST_MAIN(MessageLogModelTest)

#include "tst_messagelogmodel.moc"