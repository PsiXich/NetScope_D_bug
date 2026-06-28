#ifndef NETSCOPE_MQTTCLIENT_H
#define NETSCOPE_MQTTCLIENT_H

#include "Message.h"

#include <QObject>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QMqttTopicFilter>
#include <QMap>

// ---------------------------------------------------------------------------
// MqttClient — обёртка над QMqttClient
//
//   Брокер — всё общение идёт через посредника, не напрямую.
//      Клиент подключается к брокеру, брокер маршрутизирует сообщения
//
//   Топики — адресация строками вида "home/sensor/temperature"
//      Wildcards: "#" — любой уровень, "+" — ровно один уровень
//
//   Подписки — клиент явно говорит какие топики его интересуют
//      QMqttSubscription — живой объект, эмитирует messageReceived
//      когда брокер присылает сообщение по этому топику
//
// cleanSession = true (сессия не сохраняется
// между переподключениями).todo Persistent session — задача v2.1
//
// ---------------------------------------------------------------------------
class MqttClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected
    };
    Q_ENUM(State)

    explicit MqttClient(int connectionId, QObject *parent = nullptr);
    ~MqttClient() override;

    // --- Запросы состояния ---
    State   state()        const;
    int     connectionId() const;
    QString brokerHost()   const;
    quint16 brokerPort()   const;

    // Список активных топик-фильтров для UI панели
    QStringList subscriptions() const;

public slots:
    // Подключиться к брокеру
    // clientId — уникальный идентификатор клиента на брокере
    // Пустой clientId — брокер назначит автоматически
    // username/password — пустые если брокер не требует авторизации
    void connectToBroker(const QString &host,
                         quint16        port,
                         const QString &clientId = QString(),
                         const QString &username  = QString(),
                         const QString &password  = QString());

    // Корректное отключение с отправкой DISCONNECT пакета брокеру
    void disconnectFromBroker();

    // Подписаться на топик-фильтр
    // topicFilter поддерживает wildcards: "home/#", "sensor/+/temp"
    // qos 0 — fire & forget (достаточно для отладки)
    // qos 1 — at least once
    // qos 2 — exactly once
    // Возвращает false если не подключены или уже подписаны на этот фильтр
    bool subscribe(const QString &topicFilter, quint8 qos = 0);

    // Отписаться от топик-фильтра
    bool unsubscribe(const QString &topicFilter);

    // Опубликовать сообщение в топик
    // retain = true — брокер сохранит последнее сообщение для новых подписчиков
    bool publish(const QString    &topic,
                 const QByteArray &payload,
                 quint8            qos    = 0,
                 bool              retain = false);

signals:
    void connected   (int connectionId);
    void disconnected(int connectionId);

    // Входящее сообщение от брокера — включает топик в Message::topic
    void messageReceived(const Message &message);

    // Ошибка подключения или протокола
    void errorOccurred(int connectionId, const QString &errorString);

    // Состояние изменилось
    void stateChanged(int connectionId, MqttClient::State state);

    // Подписка добавлена / удалена — UI обновляет список подписок в панели
    void subscriptionAdded  (int connectionId, const QString &topicFilter);
    void subscriptionRemoved(int connectionId, const QString &topicFilter);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorChanged(QMqttClient::ClientError error);

    // Вызывается для каждого входящего сообщения от брокера
    void onMessageReceived(const QMqttMessage &message);

private:
    void setState(State state);

    QMqttClient                        *m_client { nullptr };

    // Храним активные подписки по топик-фильтру —
    // нужны для unsubscribe() и для отображения в UI
    QMap<QString, QMqttSubscription *>  m_subscriptions;

    int     m_connectionId;
    State   m_state { State::Disconnected };
};

#endif // NETSCOPE_MQTTCLIENT_H