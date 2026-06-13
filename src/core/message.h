#ifndef NETSCOPE_MESSAGE_H
#define NETSCOPE_MESSAGE_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

// ---------------------------------------------------------------------------
// Message — единица данных, которая передаётся между всеми слоями
// ---------------------------------------------------------------------------
struct Message
{
    // -----------------------------------------------------------------------
    // Направление передачи данных
    // -----------------------------------------------------------------------
    enum class Direction {
        Incoming,   // данные получены от удалённой стороны
        Outgoing,   // данные отправлены нами
        System      // системное событие: connect, disconnect, ошибка
    };

    // -----------------------------------------------------------------------
    // Протокол соединения — позволяет фильтровать лог по типу
    // -----------------------------------------------------------------------
    enum class Protocol {
        Tcp,
        TcpServer,  // входящее соединение на стороне сервера
        WebSocket,
        WsServer,
        Unknown
    };

    // -----------------------------------------------------------------------
    // Поля
    // -----------------------------------------------------------------------

    // Уникальный id соединения из ConnectionManager.
    // Позволяет фильтровать MessageLogModel по конкретному соединению
    int         connectionId    { -1 };

    Direction   direction       { Direction::System };
    Protocol    protocol        { Protocol::Unknown };

    // Метка времени выставляется в момент создания Message
    // Используем UTC — отображение в локальном времени делает UI-слой
    QDateTime   timestamp       { QDateTime::currentDateTimeUtc() };

    // Сырые данные. Для текстовых протоколов (WebSocket text frame,
    // JSON) здесь UTF-8 Для бинарных — произвольные байты
    // HexViewer работает с этим полем напрямую
    QByteArray  payload;

    // Человекочитаемый текст для системных событий и ошибок
    // Для Incoming/Outgoing обычно пустой — UI сам форматирует payload
    QString     info;

    // Флаг: данные являются валидным UTF-8 текстом
    // Выставляется при создании Message в сетевых классах
    // MessageLogView использует его чтобы решить: показать текст или hex
    bool        isText          { false };

    // -----------------------------------------------------------------------
    // Фабричные методы
    // -----------------------------------------------------------------------

    static Message incoming(int connId, Protocol proto, const QByteArray &data, bool text = false)
    {
        Message m;
        m.connectionId  = connId;
        m.direction     = Direction::Incoming;
        m.protocol      = proto;
        m.payload       = data;
        m.isText        = text;
        return m;
    }

    static Message outgoing(int connId, Protocol proto, const QByteArray &data, bool text = false)
    {
        Message m;
        m.connectionId  = connId;
        m.direction     = Direction::Outgoing;
        m.protocol      = proto;
        m.payload       = data;
        m.isText        = text;
        return m;
    }

    static Message system(int connId, Protocol proto, const QString &info)
    {
        Message m;
        m.connectionId  = connId;
        m.direction     = Direction::System;
        m.protocol      = proto;
        m.info          = info;
        return m;
    }
};

// ---------------------------------------------------------------------------
// Регистрация типа для Qt мета-системы
// ---------------------------------------------------------------------------
#include <QMetaType>
Q_DECLARE_METATYPE(Message)
Q_DECLARE_METATYPE(Message::Direction)
Q_DECLARE_METATYPE(Message::Protocol)

#endif // NETSCOPE_MESSAGE_H
