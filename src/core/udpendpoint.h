#ifndef NETSCOPE_UDPENDPOINT_H
#define NETSCOPE_UDPENDPOINT_H

#include "Message.h"

#include <QObject>
#include <QHostAddress>
#include <QNetworkDatagram>

class QUdpSocket;

// ---------------------------------------------------------------------------
// UdpEndpoint — отправка и приём UDP-датаграмм
//
//   - Один экземпляр одновременно принимает И отправляет
//   - Каждый вызов readDatagram() возвращает ровно одну датаграмму
//   - Нет гарантии доставки и порядка
//
//   - Broadcast поддерживается через setTargetAddress("255.255.255.255")
//     и setBroadcastEnabled(true)
// ---------------------------------------------------------------------------
class UdpEndpoint : public QObject
{
    Q_OBJECT

public:
    // Состояние — Bound означает что сокет привязан к локальному порту
    // и готов принимать датаграммы
    enum class State {
        Unbound,
        Bound
    };
    Q_ENUM(State)

    explicit UdpEndpoint(int connectionId, QObject *parent = nullptr);
    ~UdpEndpoint() override;

    // --- Запросы состояния ---
    State        state()         const;
    int          connectionId()  const;

    // Локальный порт на котором слушаем (0 если не привязан)
    quint16      localPort()     const;
    QHostAddress localAddress()  const;

    // Удалённый адрес и порт для отправки
    QString      targetAddress() const;
    quint16      targetPort()    const;

    bool         isBroadcastEnabled() const;

    // --- Настройка целевого адреса ---
    // Можно менять в любой момент — не требует перебиндинга
    void setTargetAddress(const QString &address);
    void setTargetPort(quint16 port);

    // Включить SO_BROADCAST — нужно для отправки на 255.255.255.255
    void setBroadcastEnabled(bool enabled);

public slots:
    // Привязать сокет к локальному порту для приёма датаграмм
    // port = 0 — ОС выбирает свободный порт
    // Если уже привязан — сначала unbind, потом bind заново
    bool bind(const QHostAddress &address = QHostAddress::Any,
              quint16 port = 0);

    // Освободить сокет
    void unbind();

    // Отправить данные на targetAddress:targetPort
    // Если targetAddress или targetPort не заданы — возвращает false
    // Bind не обязателен для отправки
    bool sendData(const QByteArray &data);

signals:
    void messageReceived(const Message &message);
    void errorOccurred  (int connectionId, const QString &errorString);
    void stateChanged   (int connectionId, UdpEndpoint::State state);

    // Эмитируется при каждой принятой датаграмме с адресом отправителя
    // UI может использовать это чтобы показывать "от кого" пришёл пакет
    void datagramReceived(int connectionId,
                          const QHostAddress &senderAddress,
                          quint16 senderPort,
                          const QByteArray &data);

private slots:
    void onReadyRead();

private:
    void setState(State state);

    QUdpSocket  *m_socket           { nullptr };

    int          m_connectionId;
    QString      m_targetAddress;
    quint16      m_targetPort        { 0 };
    State        m_state             { State::Unbound };
    bool         m_broadcastEnabled  { false };
};

#endif // NETSCOPE_UDPENDPOINT_H