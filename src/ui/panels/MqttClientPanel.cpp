#include "MqttClientPanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

MqttClientPanel::MqttClientPanel(int id, ConnectionManager *manager, QWidget *parent)
    : QWidget(parent), m_id(id), m_manager(manager)
{
    auto *layout = new QVBoxLayout(this);
    auto *bar = new QHBoxLayout;
    m_status = new QLabel(this);
    m_connect = new QPushButton("Connect", this);
    m_disconnect = new QPushButton("Disconnect", this);
    bar->addWidget(m_status, 1); bar->addWidget(m_connect); bar->addWidget(m_disconnect);
    layout->addLayout(bar);
    auto *tabs = new QTabWidget(this); layout->addWidget(tabs, 1);
    auto edit = [this](const char *name, const QString &value = QString()) {
        auto *field = new QLineEdit(value, this); field->setObjectName(name); return field;
    };
    auto qos = [this]() { auto *box = new QComboBox(this); box->addItems({"QoS 0", "QoS 1", "QoS 2"}); return box; };
    auto *broker = new QWidget(tabs); auto *form = new QFormLayout(broker);
    m_host = edit("mqttHost", "localhost");
    m_port = new QSpinBox(this); m_port->setObjectName("mqttPort"); m_port->setRange(1, 65535); m_port->setValue(1883);
    auto *address = new QHBoxLayout; address->addWidget(m_host, 1); address->addWidget(m_port);
    form->addRow("Broker / port", address);
    m_clientId = edit("mqttClientId"); m_clientId->setPlaceholderText("Generate automatically");
    m_username = edit("mqttUsername"); m_username->setPlaceholderText("Optional");
    m_password = edit("mqttPassword"); m_password->setEchoMode(QLineEdit::Password);
    form->addRow("Client ID", m_clientId); form->addRow("Username", m_username); form->addRow("Password", m_password);
    auto *note = new QLabel("MQTT 3.1.1 over TCP · Clean session\nSubscriptions are cleared on disconnect. TLS is not enabled.", this);
    note->setWordWrap(true); form->addRow(note); tabs->addTab(broker, "Broker");
    auto *subscriptions = new QWidget(tabs); auto *subLayout = new QVBoxLayout(subscriptions);
    m_filter = edit("mqttFilter"); m_filter->setPlaceholderText("Topic filter, e.g. home/+/temperature or home/#");
    m_subQos = qos(); m_subscribe = new QPushButton("Subscribe", this);
    auto *subRow = new QHBoxLayout; subRow->addWidget(m_filter, 1); subRow->addWidget(m_subQos); subRow->addWidget(m_subscribe);
    subLayout->addLayout(subRow); m_subscriptions = new QListWidget(this);
    m_subscriptions->setObjectName("mqttSubscriptions"); subLayout->addWidget(m_subscriptions);
    m_unsubscribe = new QPushButton("Unsubscribe selected", this); subLayout->addWidget(m_unsubscribe);
    tabs->addTab(subscriptions, "Subscriptions");
    auto *publish = new QWidget(tabs); auto *pubLayout = new QVBoxLayout(publish);
    m_topic = edit("mqttTopic"); m_topic->setPlaceholderText("Publish topic, e.g. home/lamp/state"); pubLayout->addWidget(m_topic);
    m_payload = new QPlainTextEdit(this); m_payload->setObjectName("mqttPayload");
    m_payload->setPlaceholderText("Message (UTF-8). Empty payloads are allowed."); pubLayout->addWidget(m_payload, 1);
    auto *pubRow = new QHBoxLayout; m_pubQos = qos(); m_retain = new QCheckBox("Retain", this); m_hex = new QCheckBox("HEX input", this);
    m_retain->setToolTip("Store the last message on the broker. An empty retained message clears it.");
    m_publish = new QPushButton("Publish", this);
    pubRow->addWidget(m_pubQos); pubRow->addWidget(m_retain); pubRow->addWidget(m_hex); pubRow->addStretch(); pubRow->addWidget(m_publish);
    pubLayout->addLayout(pubRow); tabs->addTab(publish, "Publish");
    m_feedback = new QLabel(this); m_feedback->setWordWrap(true); m_feedback->setTextFormat(Qt::PlainText); layout->addWidget(m_feedback);
    connect(m_connect, &QPushButton::clicked, this, [this] {
        const QString host = m_host->text().trimmed();
        if (host.isEmpty() || host.contains("://") || host.contains('/')) {
            m_feedback->setText("Enter a hostname or IP address without a URL scheme or path."); return;
        }
        m_feedback->clear();
        m_manager->connectMqttClient(m_id, host, quint16(m_port->value()), m_clientId->text(), m_username->text(), m_password->text()); refresh();
    });
    connect(m_disconnect, &QPushButton::clicked, this, [this] { m_manager->disconnectMqttClient(m_id); refresh(); });
    connect(m_subscribe, &QPushButton::clicked, this, [this] {
        m_feedback->setText(m_manager->subscribeMqtt(m_id, m_filter->text(), quint8(m_subQos->currentIndex()))
            ? "Subscription requested; waiting for broker confirmation." : "Cannot subscribe: check the filter and existing subscriptions.");
    });
    connect(m_unsubscribe, &QPushButton::clicked, this, [this] {
        auto *item = m_subscriptions->currentItem();
        if (item && m_manager->unsubscribeMqtt(m_id, item->text())) m_feedback->setText("Unsubscribe requested; waiting for broker confirmation.");
    });
    connect(m_publish, &QPushButton::clicked, this, [this] {
        QByteArray bytes;
        if (m_hex->isChecked()) {
            QString hex = m_payload->toPlainText(); hex.remove(QRegularExpression("\\s"));
            if (hex.size() % 2 || hex.contains(QRegularExpression("[^0-9a-fA-F]"))) {
                m_feedback->setText("HEX must contain complete byte pairs, e.g. 48 65 6C 6C 6F."); return;
            }
            bytes = QByteArray::fromHex(hex.toLatin1());
        } else bytes = m_payload->toPlainText().toUtf8();
        m_feedback->setText(m_manager->publishMqtt(m_id, m_topic->text(), bytes, quint8(m_pubQos->currentIndex()), m_retain->isChecked())
            ? "Message queued for publishing." : "Cannot publish: use a valid topic without + or # and check the connection.");
    });
    connect(manager, &ConnectionManager::connectionInfoChanged, this, [this](int changed) { if (changed == m_id) refresh(); });
    connect(manager, &ConnectionManager::mqttSubscriptionAdded, this, [this](int changed, const QString &topic) {
        if (changed == m_id) { refresh(); m_feedback->setText("Subscribed: " + topic); }
    });
    connect(manager, &ConnectionManager::mqttSubscriptionRemoved, this, [this](int changed, const QString &topic) {
        if (changed == m_id) { refresh(); m_feedback->setText("Subscription removed: " + topic); }
    });
    connect(manager, &ConnectionManager::mqttError, this, [this](int changed, const QString &error) {
        if (changed == m_id) { refresh(); m_feedback->setText("Error: " + error); }
    });
    connect(m_subscriptions, &QListWidget::itemSelectionChanged, this, [this] {
        m_unsubscribe->setEnabled(m_manager->mqttState(m_id) == MqttClient::State::Connected && m_subscriptions->currentItem());
    });
    refresh();
}
void MqttClientPanel::refresh()
{
    const auto state = m_manager->mqttState(m_id);
    const bool idle = state == MqttClient::State::Disconnected, connected = state == MqttClient::State::Connected;
    m_status->setText(connected ? "Connected" : idle ? "Disconnected" : "Connecting…");
    m_connect->setEnabled(idle); m_disconnect->setEnabled(!idle);
    for (auto *field : {m_host, m_clientId, m_username, m_password}) field->setEnabled(idle);
    m_port->setEnabled(idle); m_subscribe->setEnabled(connected); m_publish->setEnabled(connected);
    const QString selected = m_subscriptions->currentItem() ? m_subscriptions->currentItem()->text() : QString();
    m_subscriptions->clear(); m_subscriptions->addItems(m_manager->mqttSubscriptions(m_id));
    const auto matches = m_subscriptions->findItems(selected, Qt::MatchExactly);
    if (!matches.isEmpty()) m_subscriptions->setCurrentItem(matches.first());
    m_unsubscribe->setEnabled(connected && m_subscriptions->currentItem());
}
