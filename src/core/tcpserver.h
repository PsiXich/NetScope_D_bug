#ifndef NETSCOPE_TCPSERVER_H
#define NETSCOPE_TCPSERVER_H

#include "Message.h"

#include <QObject>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QMap>

class QTcpServer;
class QTcpSocket;

// ---------------------------------------------------------------------------
// ClientSession — описывает одно входящее соединение на стороне сервера
//
// Хранится в QMap по socketDescriptor
// ---------------------------------------------------------------------------
struct ClientSession
{
    qintptr     descriptor { -1 };      // уникальный id сокета от ОС
    QString     address;                // удалённый IP
    quint16     port        { 0 };      // удалённый порт
    QTcpSocket *socket      { nullptr };

    // Удобный строковый идентификатор для логов и UI
    QString displayName() const
    {
        return QString("%1:%2").arg(address).arg(port);
    }
};

// ---------------------------------------------------------------------------
// TcpServer — принимает входящие TCP-соединения и управляет сессиями
//
// Ответственность:
//   - слушать порт и принимать входящие соединения
//   - рассылать данные одному или всем клиентам
//   - уведомлять подписчиков о новых сессиях, данных и отключениях
//
// Каждая ClientSession идентифицируется по socketDescriptor (qintptr)
// ---------------------------------------------------------------------------
class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(int connectionId, QObject *parent = nullptr);
    ~TcpServer() override;

    // --- Запросы состояния ---
    bool        isListening()   const;
    int         connectionId()  const;
    quint16     port()          const;
    QHostAddress address()      const;
    int         clientCount()   const;

    // Список активных сессий — для отображения в UI
    QList<ClientSession> sessions() const;

public slots:
    // Начать прослушивание address = QHostAddress::Any — все интерфейсы
    // port = 0 — ОС выбирает свободный порт (удобно для тестов)
    bool startListening(const QHostAddress &address = QHostAddress::Any,
                        quint16 port = 0);

    // Остановить сервер и закрыть все активные сессии
    void stopListening();

    // Отправить данные конкретному клиенту по его socketDescriptor
    bool sendToClient(qintptr descriptor, const QByteArray &data);

    // Отправить данные всем подключённым клиентам
    void broadcast(const QByteArray &data);

signals:
    // Новый клиент подключился
    void clientConnected(qintptr descriptor, const QString &displayName);

    // Клиент отключился
    void clientDisconnected(qintptr descriptor, const QString &displayName);

    // Получены данные от клиента
    void messageReceived(const Message &message);

    // Ошибка сервера (не клиентского сокета)
    void errorOccurred(int connectionId, const QString &errorString);

    // Сервер начал / прекратил слушать
    void listeningChanged(bool listening);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    // Найти сессию по сокету-отправителю сигнала (sender())
    ClientSession *sessionBySender();

    // Закрыть и удалить сессию
    void removeSession(qintptr descriptor);

    QTcpServer              *m_server       { nullptr };
    QMap<qintptr, ClientSession> m_sessions;   // descriptor → session

    int                      m_connectionId;
};

#endif // NETSCOPE_TCPSERVER_H