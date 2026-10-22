#include "MqttClient.h"

#include <QMqttMessage>
#include <QDebug>
#include <QUuid>

// ---------------------------------------------------------------------------
// MqttClient implementation
// ---------------------------------------------------------------------------

MqttClient::MqttClient(int connectionId, QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_connectionId(connectionId)
{
    // cleanSession = true — не сохраняем сессию между переподключениями
    // Брокер не будет ставить в очередь пропущенные сообщения
    m_client->setCleanSession(true);
    m_client->setProtocolVersion(QMqttClient::MQTT_3_1_1);
    connect(m_client, &QMqttClient::messageReceived,
            this, &MqttClient::onMessageReceived);

    connect(m_client, &QMqttClient::connected,
            this, &MqttClient::onConnected);

    connect(m_client, &QMqttClient::disconnected,
            this, &MqttClient::onDisconnected);

    // errorChanged — единственный сигнал об ошибках в QMqttClient
    // Не перегружен, используем новый синтаксис
    connect(m_client, &QMqttClient::errorChanged,
            this, &MqttClient::onErrorChanged);

    connect(m_client, &QMqttClient::stateChanged,
            this, &MqttClient::onClientStateChanged);
}

MqttClient::~MqttClient()
{
    // Clean session subscriptions disappear when the connection closes.
    // Do not emit callbacks while this object is being destroyed.
    m_client->disconnect(this);
    for (auto *sub : m_subscriptions) sub->disconnect(this);
    m_subscriptions.clear();

    // QMqttClient удалится автоматически (parent = this)
    // Отправляем DISCONNECT если ещё подключены
    if (m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
    }
}

// ---------------------------------------------------------------------------
// Запросы состояния
// ---------------------------------------------------------------------------

MqttClient::State MqttClient::state() const
{
    return fromQtState(m_client->state());
}

int MqttClient::connectionId() const
{
    return m_connectionId;
}

QString MqttClient::brokerHost() const
{
    return m_client->hostname();
}

quint16 MqttClient::brokerPort() const
{
    return m_client->port();
}

QStringList MqttClient::subscriptions() const
{
    QStringList active;
    for (auto it = m_subscriptions.cbegin(); it != m_subscriptions.cend(); ++it)
        if (it.value()->state() == QMqttSubscription::Subscribed)
            active.append(it.key());
    return active;
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void MqttClient::connectToBroker(const QString &host,
                                 quint16        port,
                                 const QString &clientId,
                                 const QString &username,
                                 const QString &password)
{
    if (state() != State::Disconnected) {
        qDebug() << "[MqttClient] id=" << m_connectionId
                 << "already connecting or connected, ignoring.";
        return;
    }

    if (host.trimmed().isEmpty() || port == 0) {
        emit errorOccurred(m_connectionId, "Enter a broker host and a valid port.");
        return;
    }
    m_client->setHostname(host.trimmed());
    m_client->setPort(port);

    // Если clientId пустой — генерируем уникальный
    // Брокеры требуют уникальный ClientID, пустой может быть отклонён
    if (clientId.isEmpty()) {
        m_client->setClientId(
            QString("netscope-%1").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                )
            );
    } else {
        m_client->setClientId(clientId);
    }

    // Устанавливаем учётные данные
    m_client->setUsername(username);
    m_client->setPassword(password);

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "connecting to" << host << "port" << port
             << "clientId:" << m_client->clientId();

    m_client->connectToHost();
}

void MqttClient::disconnectFromBroker()
{
    if (m_client->state() == QMqttClient::Disconnected) {
        return;
    }

    // Отправляем DISCONNECT пакет — корректное завершение сессии MQTT
    m_client->disconnectFromHost();
}

bool MqttClient::subscribe(const QString &topicFilter, quint8 qos)
{
    if (state() != State::Connected) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe() called while not connected.";
        return false;
    }

    if (qos > 2) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe(): invalid QoS:" << qos;
        return false;
    }

    const QMqttTopicFilter filter(topicFilter);

    if (!filter.isValid()) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe(): invalid topic filter:"
                   << topicFilter;
        return false;
    }

    // Не подписываемся повторно на уже активный фильтр
    if (m_subscriptions.contains(topicFilter)) {
        qDebug() << "[MqttClient] id=" << m_connectionId
                 << "already subscribed to" << topicFilter;
        return false;
    }

    QMqttSubscription *sub = m_client->subscribe(filter, qos);

    if (!sub) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe failed for" << topicFilter;
        return false;
    }

    m_subscriptions.insert(topicFilter, sub);

    // Only report success after SUBACK; a broker may reject the subscription.
    connect(sub, &QMqttSubscription::stateChanged, this,
            [this, sub, topicFilter](QMqttSubscription::SubscriptionState state) {
        if (state == QMqttSubscription::Subscribed) {
            emit messageReceived(Message::system(m_connectionId, Message::Protocol::Mqtt,
                QString("Subscribed to '%1' (QoS %2)").arg(topicFilter).arg(sub->qos())));
            emit subscriptionAdded(m_connectionId, topicFilter);
        } else if (state == QMqttSubscription::Error) {
            // Qt keeps a rejected subscription cached until UNSUBACK.
            // Retain our entry during cleanup to prevent duplicate retries.
            sub->setProperty("netscopeRejected", true);
            sub->unsubscribe();
            emit errorOccurred(m_connectionId, QString("Subscription rejected: %1").arg(topicFilter));
        } else if (state == QMqttSubscription::Unsubscribed) {
            m_subscriptions.remove(topicFilter);
            sub->disconnect(this);
            if (!sub->property("netscopeRejected").toBool())
                emit subscriptionRemoved(m_connectionId, topicFilter);
        }
    });

    return true;
}

bool MqttClient::unsubscribe(const QString &topicFilter)
{
    if (!m_subscriptions.contains(topicFilter)) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "unsubscribe: not subscribed to" << topicFilter;
        return false;
    }

    QMqttSubscription *sub = m_subscriptions.value(topicFilter);

    if (sub) {
        sub->unsubscribe();
        // sub принадлежит QMqttClient, не удаляем вручную
    }

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "unsubscribed from" << topicFilter;

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Mqtt,
        QString("Unsubscribed from '%1'").arg(topicFilter)
        ));

    return true;
}

bool MqttClient::publish(const QString    &topic,
                         const QByteArray &payload,
                         quint8            qos,
                         bool              retain)
{
    if (state() != State::Connected) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish() called while not connected.";
        return false;
    }

    if (topic.isEmpty()) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish(): topic is empty.";
        return false;
    }

    if (qos > 2) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish(): invalid QoS:" << qos;
        return false;
    }

    const QMqttTopicName topicName(topic);

    if (!topicName.isValid()) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish(): invalid topic:" << topic;
        return false;
    }

    const qint32 result = m_client->publish(
        topicName,
        payload,
        qos,
        retain
        );

    if (result == -1) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish() failed for topic:" << topic;
        return false;
    }

    // Логируем исходящее сообщение с топиком
    emit messageReceived(Message::outgoing(
        m_connectionId,
        Message::Protocol::Mqtt,
        payload,
        !payload.contains('\0') && QString::fromUtf8(payload).toUtf8() == payload,
        topic
        ));

    return true;
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void MqttClient::onConnected()
{

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "connected to" << m_client->hostname()
             << "port" << m_client->port();

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Mqtt,
        QString("Connected to %1:%2  (clientId: %3)")
            .arg(m_client->hostname())
            .arg(m_client->port())
            .arg(m_client->clientId())
        ));
    emit connected(m_connectionId);
}

void MqttClient::onDisconnected()
{
    // Подписки инвалидируются при отключении — очищаем карту
    // subscriptionRemoved не эмитируем для каждой — это было бы
    // избыточно, UI и так знает что при disconnect всё сбрасывается
    m_subscriptions.clear();

    qDebug() << "[MqttClient] id=" << m_connectionId << "disconnected";

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Mqtt,
        QString("Disconnected from %1:%2")
            .arg(m_client->hostname())
            .arg(m_client->port())
        ));
    emit disconnected(m_connectionId);
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError) {
        return;
    }

    // Получаем описание ошибки
    QString errStr;
    switch (error) {
    case QMqttClient::InvalidProtocolVersion:
        errStr = "Invalid protocol version";        break;
    case QMqttClient::IdRejected:
        errStr = "Client ID rejected by broker";    break;
    case QMqttClient::ServerUnavailable:
        errStr = "Broker unavailable";              break;
    case QMqttClient::BadUsernameOrPassword:
        errStr = "Bad username or password";        break;
    case QMqttClient::NotAuthorized:
        errStr = "Not authorized";                  break;
    case QMqttClient::TransportInvalid:
        errStr = "Transport connection failed";     break;
    case QMqttClient::ProtocolViolation:
        errStr = "Protocol violation";              break;
    case QMqttClient::UnknownError:
    default:
        errStr = "Unknown MQTT error";              break;
    }

    qWarning() << "[MqttClient] id=" << m_connectionId
               << "error:" << errStr;

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Mqtt,
        QString("Error: %1").arg(errStr)
        ));
    emit errorOccurred(m_connectionId, errStr);
}

void MqttClient::onMessageReceived(const QByteArray &data, const QMqttTopicName &topicName)
{
    const QString topic = topicName.name();

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "message on topic:" << topic
             << "size:" << data.size();

    // Передаём Message с заполненным полем topic —
    // MessageLogModel и фильтр по топику используют его напрямую
    emit messageReceived(Message::incoming(
        m_connectionId,
        Message::Protocol::Mqtt,
        data,
        !data.contains('\0') && QString::fromUtf8(data).toUtf8() == data,
        topic
        ));
}

void MqttClient::onClientStateChanged(
    QMqttClient::ClientState state)
{
    if (state == QMqttClient::Disconnected) {
        for (auto *sub : m_subscriptions)
            sub->disconnect(this);
        m_subscriptions.clear();
    }
    emit stateChanged(m_connectionId, fromQtState(state));
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

MqttClient::State MqttClient::fromQtState(
    QMqttClient::ClientState state)
{
    switch (state) {
    case QMqttClient::Disconnected:
        return State::Disconnected;

    case QMqttClient::Connecting:
        return State::Connecting;

    case QMqttClient::Connected:
        return State::Connected;
    }

    return State::Disconnected;
}
