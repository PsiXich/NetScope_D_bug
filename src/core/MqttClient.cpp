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

    connect(m_client, &QMqttClient::connected,
            this, &MqttClient::onConnected);

    connect(m_client, &QMqttClient::disconnected,
            this, &MqttClient::onDisconnected);

    // errorChanged — единственный сигнал об ошибках в QMqttClient
    // Не перегружен, используем новый синтаксис
    connect(m_client, &QMqttClient::errorChanged,
            this, &MqttClient::onErrorChanged);
}

MqttClient::~MqttClient()
{
    // Явно отписываемся от всех топиков перед удалением —
    // это освобождает ресурсы на стороне брокера
    const QStringList filters = m_subscriptions.keys();
    for (const QString &filter : filters) {
        if (m_subscriptions[filter]) {
            m_subscriptions[filter]->unsubscribe();
        }
    }
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
    return m_state;
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
    return m_subscriptions.keys();
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
    if (m_state != State::Disconnected) {
        qDebug() << "[MqttClient] id=" << m_connectionId
                 << "already connecting or connected, ignoring.";
        return;
    }

    m_client->setHostname(host);
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

    // Устанавливаем учётные данные если заданы
    if (!username.isEmpty()) {
        m_client->setUsername(username);
        m_client->setPassword(password);
    }

    setState(State::Connecting);

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
    if (m_state != State::Connected) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe() called while not connected.";
        return false;
    }

    if (topicFilter.isEmpty()) {
        return false;
    }

    // Не подписываемся повторно на уже активный фильтр
    if (m_subscriptions.contains(topicFilter)) {
        qDebug() << "[MqttClient] id=" << m_connectionId
                 << "already subscribed to" << topicFilter;
        return false;
    }

    QMqttTopicFilter filter(topicFilter);
    QMqttSubscription *sub = m_client->subscribe(filter, qos);

    if (!sub) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "subscribe failed for" << topicFilter;
        return false;
    }

    m_subscriptions.insert(topicFilter, sub);

    // Подключаем сигнал входящего сообщения от этой подписки
    connect(sub, &QMqttSubscription::messageReceived,
            this, &MqttClient::onMessageReceived);

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "subscribed to" << topicFilter << "QoS" << qos;

    emit messageReceived(Message::system(
        m_connectionId,
        Message::Protocol::Mqtt,
        QString("Subscribed to '%1' (QoS %2)").arg(topicFilter).arg(qos)
        ));
    emit subscriptionAdded(m_connectionId, topicFilter);

    return true;
}

bool MqttClient::unsubscribe(const QString &topicFilter)
{
    if (!m_subscriptions.contains(topicFilter)) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "unsubscribe: not subscribed to" << topicFilter;
        return false;
    }

    QMqttSubscription *sub = m_subscriptions.take(topicFilter);

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
    emit subscriptionRemoved(m_connectionId, topicFilter);

    return true;
}

bool MqttClient::publish(const QString    &topic,
                         const QByteArray &payload,
                         quint8            qos,
                         bool              retain)
{
    if (m_state != State::Connected) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish() called while not connected.";
        return false;
    }

    if (topic.isEmpty()) {
        qWarning() << "[MqttClient] id=" << m_connectionId
                   << "publish(): topic is empty.";
        return false;
    }

    if (payload.isEmpty()) {
        return false;
    }

    const qint32 result = m_client->publish(
        QMqttTopicName(topic),
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
        false,  // isText определяет получатель — UI может переключить
        topic
        ));

    return true;
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void MqttClient::onConnected()
{
    setState(State::Connected);

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

    setState(State::Disconnected);

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

    // При ошибке переходим в Disconnected
    setState(State::Disconnected);
}

void MqttClient::onMessageReceived(const QMqttMessage &message)
{
    const QString topic   = message.topic().name();
    const QByteArray data = message.payload();

    qDebug() << "[MqttClient] id=" << m_connectionId
             << "message on topic:" << topic
             << "size:" << data.size();

    // Передаём Message с заполненным полем topic —
    // MessageLogModel и фильтр по топику используют его напрямую
    emit messageReceived(Message::incoming(
        m_connectionId,
        Message::Protocol::Mqtt,
        data,
        false,  // isText: MQTT не различает text/binary на уровне протокола
        topic
        ));
}

// ---------------------------------------------------------------------------
// Приватные вспомогательные методы
// ---------------------------------------------------------------------------

void MqttClient::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(m_connectionId, m_state);
}