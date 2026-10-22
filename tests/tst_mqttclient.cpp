#include <QTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QListWidget>
#include "core/MqttClient.h"
#include "ui/panels/MqttClientPanel.h"
#include "models/MessageLogModel.h"

// Small, deterministic MQTT 3.1.1 peer. No external broker or internet required.
// It parses full MQTT frames, including fragmented TCP reads and QoS handshakes.
class Broker : public QTcpServer {
public:
    QByteArray connectPacket, lastPayload;
    QString lastTopic;
    int publications = 0;
    bool rejectLogin = false, rejectSubscription = false;
    QTcpSocket *peer = nullptr;
    Broker() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            peer = nextPendingConnection();
            auto *socket = peer;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer = QByteArray()]() mutable {
                buffer += socket->readAll();
                while (buffer.size() >= 2) {
                    int length = 0, multiplier = 1, pos = 1;
                    quint8 digit;
                    do {
                        if (pos >= buffer.size()) return;
                        digit = quint8(buffer[pos++]); length += (digit & 127) * multiplier; multiplier *= 128;
                    } while (digit & 128);
                    if (buffer.size() < pos + length) return;
                    const quint8 header = quint8(buffer[0]);
                    const QByteArray body = buffer.mid(pos, length); buffer.remove(0, pos + length);
                    switch (header >> 4) {
                    case 1:
                        connectPacket = body;
                        socket->write(rejectLogin ? QByteArray::fromHex("20020004") : QByteArray::fromHex("20020000")); break;
                    case 8:
                        send(socket, 0x90, body.left(2) + char(rejectSubscription ? 0x80 : quint8(body.back()))); break;
                    case 10: send(socket, 0xb0, body.left(2)); break;
                    case 3: {
                        const int topicLength = quint8(body[0]) * 256 + quint8(body[1]);
                        lastTopic = QString::fromUtf8(body.mid(2, topicLength));
                        const int qos = (header >> 1) & 3;
                        lastPayload = body.mid(2 + topicLength + (qos ? 2 : 0)); ++publications;
                        if (qos) send(socket, qos == 1 ? 0x40 : 0x50, body.mid(2 + topicLength, 2));
                        break;
                    }
                    case 6: send(socket, 0x70, body.left(2)); break;
                    case 12: socket->write(QByteArray::fromHex("d000")); break;
                    case 14: socket->disconnectFromHost(); break;
                    default: break;
                    }
                }
            });
        });
    }
    static void send(QTcpSocket *socket, quint8 header, const QByteArray &body) {
        QByteArray packet(1, char(header)); int remaining = body.size();
        do { quint8 digit = remaining % 128; remaining /= 128; if (remaining) digit |= 128; packet += char(digit); } while (remaining);
        socket->write(packet + body);
    }
    void deliver(const QString &topic, const QByteArray &payload) {
        const auto name = topic.toUtf8(); QByteArray body;
        body += char(name.size() >> 8); body += char(name.size() & 255);
        send(peer, 0x30, body + name + payload);
    }
};

class MqttClientTest : public QObject {
    Q_OBJECT
private slots:
    void lifecycle() {
        Broker broker; QVERIFY(broker.listen(QHostAddress::LocalHost));
        MqttClient client(1);
        QSignalSpy added(&client, &MqttClient::subscriptionAdded), removed(&client, &MqttClient::subscriptionRemoved);
        client.connectToBroker("127.0.0.1", broker.serverPort(), "netscope-test", "user", "secret");
        QTRY_COMPARE(client.state(), MqttClient::State::Connected);
        QVERIFY(broker.connectPacket.contains("netscope-test")); QVERIFY(broker.connectPacket.contains("user")); QVERIFY(broker.connectPacket.contains("secret"));
        QVERIFY(client.subscribe("home/#", 1)); QVERIFY(client.subscriptions().isEmpty());
        QTRY_COMPARE(added.count(), 1); QCOMPARE(client.subscriptions(), QStringList{"home/#"});
        QVERIFY(!client.subscribe("home/#")); QVERIFY(!client.subscribe("bad/#/topic"));
        QVERIFY(client.subscribe("home/+", 0)); QTRY_COMPARE(added.count(), 2);
        QSignalSpy messages(&client, &MqttClient::messageReceived);
        broker.deliver("home/lamp", "hello"); QTRY_COMPARE(messages.count(), 1);
        auto incoming = qvariant_cast<Message>(messages.first().first());
        QCOMPARE(incoming.topic, QString("home/lamp")); QCOMPARE(incoming.payload, QByteArray("hello")); QVERIFY(incoming.isText);
        for (int qos = 0; qos <= 2; ++qos) {
            QVERIFY(client.publish("home/lamp", QByteArray(256, 'x'), quint8(qos), true));
            QTRY_COMPARE(broker.publications, qos + 1); QCOMPARE(broker.lastPayload.size(), 256);
        }
        QVERIFY(client.publish("home/lamp", {}, 1, true)); QTRY_COMPARE(broker.publications, 4); QVERIFY(broker.lastPayload.isEmpty());
        QVERIFY(!client.publish("home/+", "bad")); QVERIFY(!client.publish("home/lamp", "bad", 3));
        QVERIFY(client.unsubscribe("home/#")); QTRY_COMPARE(removed.count(), 1);
        QVERIFY(client.subscribe("home/#")); QTRY_COMPARE(added.count(), 3);
        client.disconnectFromBroker(); QTRY_COMPARE(client.state(), MqttClient::State::Disconnected); QVERIFY(client.subscriptions().isEmpty());
        client.connectToBroker("127.0.0.1", broker.serverPort()); QTRY_COMPARE(client.state(), MqttClient::State::Connected);
        QVERIFY(!broker.connectPacket.contains("secret"));
        QVERIFY(client.subscribe("home/#")); QTRY_COMPARE(added.count(), 4);
        messages.clear(); broker.deliver("home/lamp", "again"); QTRY_COMPARE(messages.count(), 1);
        broker.peer->abort(); QTRY_COMPARE(client.state(), MqttClient::State::Disconnected); QVERIFY(client.subscriptions().isEmpty());
    }
    void rejections() {
        Broker broker; QVERIFY(broker.listen(QHostAddress::LocalHost)); broker.rejectLogin = true;
        MqttClient client(2); QSignalSpy errors(&client, &MqttClient::errorOccurred);
        client.connectToBroker("127.0.0.1", broker.serverPort()); QTRY_VERIFY(!errors.isEmpty());
        QCOMPARE(client.state(), MqttClient::State::Disconnected);
        broker.rejectLogin = false; broker.rejectSubscription = true;
        client.connectToBroker("127.0.0.1", broker.serverPort()); QTRY_COMPARE(client.state(), MqttClient::State::Connected);
        errors.clear(); QVERIFY(client.subscribe("denied/#")); QTRY_COMPARE(errors.count(), 1); QVERIFY(client.subscriptions().isEmpty());
        broker.rejectSubscription = false;
        bool retried = false;
        QTRY_VERIFY(retried || (retried = client.subscribe("denied/#")));
        QTRY_COMPARE(client.subscriptions().size(), 1);
    }
    void panelWorkflow() {
        Broker broker; QVERIFY(broker.listen(QHostAddress::LocalHost));
        ConnectionManager manager; const int id = manager.createMqttClient();
        MqttClientPanel panel(id, &manager); panel.resize(720, 360); panel.show();
        auto button = [&panel](const QString &text) -> QPushButton * {
            for (auto *b : panel.findChildren<QPushButton *>()) if (b->text() == text) return b;
            return nullptr;
        };
        QVERIFY(!button("Publish")->isEnabled());
        panel.findChild<QLineEdit *>("mqttHost")->setText("127.0.0.1");
        panel.findChild<QSpinBox *>("mqttPort")->setValue(broker.serverPort()); button("Connect")->click();
        QTRY_VERIFY(manager.connectionInfo(id).isActive); QVERIFY(button("Publish")->isEnabled());
        auto *tabs = panel.findChild<QTabWidget *>(); tabs->setCurrentIndex(1);
        panel.findChild<QLineEdit *>("mqttFilter")->setText("test/#"); button("Subscribe")->click();
        auto *list = panel.findChild<QListWidget *>("mqttSubscriptions"); QTRY_COMPARE(list->count(), 1);
        tabs->setCurrentIndex(2); panel.findChild<QLineEdit *>("mqttTopic")->setText("test/value");
        panel.findChild<QPlainTextEdit *>("mqttPayload")->setPlainText("Hello MQTT"); button("Publish")->click();
        QTRY_COMPARE(broker.lastPayload, QByteArray("Hello MQTT"));
        const QString screenshot = qEnvironmentVariable("NETSCOPE_MQTT_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(panel.grab().save(screenshot));
        tabs->setCurrentIndex(1); list->setCurrentRow(0); button("Unsubscribe selected")->click(); QTRY_COMPARE(list->count(), 0);
        button("Disconnect")->click(); QTRY_VERIFY(!manager.connectionInfo(id).isActive); QVERIFY(!button("Publish")->isEnabled());
    }
    void topicLog() {
        MessageLogModel model;
        model.appendMessage(Message::incoming(1, Message::Protocol::Mqtt, "value", true, "home/lamp"));
        QCOMPARE(model.data(model.index(0, MessageLogModel::ColTopic)).toString(), QString("home/lamp"));
        model.setFilterTopic("home/+"); QCOMPARE(model.rowCount(), 1);
        model.setFilterTopic("other/#"); QCOMPARE(model.rowCount(), 0);
    }
};
QTEST_MAIN(MqttClientTest)
#include "tst_mqttclient.moc"
