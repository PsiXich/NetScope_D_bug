#ifndef NETSCOPE_TCPCLIENT_H
#define NETSCOPE_TCPCLIENT_H

#include "Message.h"

#include <QObject>
#include <QAbstractSocket>
#include <QHostAddress>

class QTcpSocket;
class QTimer;

// ---------------------------------------------------------------------------
// TcpClient — управляет одним исходящим TCP-соединением
//
// Ответственность класса (Single Responsibility):
//   - установить / разорвать соединение
//   - отправить данные
//   - уведомить подписчиков через сигналы о входящих данных и событиях

// todo
// Если в будущем понадобится вынести в QThread —
// достаточно moveToThread() без изменения интерфейса
// ---------------------------------------------------------------------------
class TcpClient : public QObject
{
    Q_OBJECT

public:
    // Состояния клиента — отражают жизненный цикл соединения
    // Используется в UI для отображения статуса и блокировки кнопок
    enum class State {
        Disconnected,
        Connecting,
        Connected
    };
    Q_ENUM(State)   // регистрируем для QMetaEnum / отладки

    explicit TcpClient(int connectionId, QObject *parent = nullptr);
    ~TcpClient() override;

    // --- Запросы состояния ---
    State       state()         const;
    int         connectionId()  const;
    QString     host()          const;
    quint16     port()          const;

    // Интервал переподключения, мс. 0 = автореконнект выключен
    int         reconnectInterval() const;
    void        setReconnectInterval(int ms);

public slots:
    // Инициировать подключение. Если уже подключены — игнорируется
    void connectToHost(const QString &host, quint16 port);

    // Корректное закрытие: сначала flush, потом disconnectFromHost
    // После вызова автореконнект отключается
    void disconnectFromHost();

    // Отправить данные Возвращает false если не подключены
    // Данные буферизуются QTcpSocket — не блокирует
    bool sendData(const QByteArray &data);

signals:
    // Соединение установлено
    void connected(int connectionId);

    // Соединение разорвано (в том числе по ошибке)
    void disconnected(int connectionId);

    // Получены данные от удалённой стороны
    void messageReceived(const Message &message);

    // Произошла ошибка сокета
    void errorOccurred(int connectionId, const QString &errorString);

    // Состояние изменилось — UI подписывается чтобы обновить кнопки
    void stateChanged(int connectionId, TcpClient::State state);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimer();

private:
    void setState(State state);
    void setupReconnect();
    void teardownReconnect();

    QTcpSocket  *m_socket           { nullptr };
    QTimer      *m_reconnectTimer   { nullptr };

    int         m_connectionId;
    QString     m_host;
    quint16     m_port              { 0 };
    State       m_state             { State::Disconnected };
    int         m_reconnectInterval { 0 };

    // Флаг: пользователь явно вызвал disconnectFromHost()
    // Нужен чтобы отличить намеренный разрыв от обрыва соединения
    // и не запускать автореконнект в первом случае
    bool        m_intentionalDisconnect { false };
};

#endif // NETSCOPE_TCPCLIENT_H