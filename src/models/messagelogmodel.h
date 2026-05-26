
#ifndef NETSCOPE_MESSAGELOGMODEL_H
#define NETSCOPE_MESSAGELOGMODEL_H

#include "core/Message.h"

#include <QAbstractTableModel>
#include <QVector>
#include <QColor>

// ---------------------------------------------------------------------------
// MessageLogModel — табличная модель для отображения лога сообщений
//
// Наследуем QAbstractTableModel а не QStandardItemModel потому что:
//   - полный контроль над данными и их хранением
//   - нет копирования данных в QStandardItem — Message хранится напрямую
//   - можно оптимизировать beginInsertRows() / endInsertRows() под наши нужды
//   - возможность добавить виртуальный скролл (ограничение числа строк)
//     без переписывания интерфейса
//
// Колонки:
//   0 — Time      метка времени (UTC → local при отображении)
//   1 — ID        connectionId
//   2 — Protocol  tcp / tcp-server / websocket
//   3 — Direction  Incoming /  Outgoing /  System
//   4 — Size      размер payload в байтах
//   5 — Data      превью данных (первые N символов)
//
// Фильтрация: реализована внутри модели через m_filter —
// QSortFilterProxyModel не используем чтобы не дублировать данные
// При изменении фильтра делаем полный reset
// ---------------------------------------------------------------------------
class MessageLogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // Индексы колонок — используем enum чтобы не хардкодить числа в UI
    enum Column {
        ColTime      = 0,
        ColId        = 1,
        ColProtocol  = 2,
        ColDirection = 3,
        ColSize      = 4,
        ColData      = 5,
        ColCount     = 6    // всегда последний — используется в columnCount()
    };

    // Кастомные роли для доступа к сырым данным из делегата
    enum CustomRole {
        RawMessageRole  = Qt::UserRole + 1,  // возвращает Message целиком
        DirectionRole   = Qt::UserRole + 2,  // возвращает Direction для окраски
        IsTextRole      = Qt::UserRole + 3   // возвращает bool isText
    };

    explicit MessageLogModel(QObject *parent = nullptr);

    // --- QAbstractTableModel interface ---
    int      rowCount   (const QModelIndex &parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data       (const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData (int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // --- Управление данными ---

    // Максимальное число строк При превышении старые строки удаляются
    // 0 = без ограничения По умолчанию 10000
    int  maxRowCount() const;
    void setMaxRowCount(int count);

    // Получить сообщение по индексу строки
    Message messageAt(int row) const;

    // --- Фильтрация ---
    // Фильтр по connectionId -1 = показывать все соединения
    int  filterConnectionId() const;
    void setFilterConnectionId(int id);

    // Фильтр по протоколу Protocol::Unknown = показывать все
    Message::Protocol filterProtocol() const;
    void              setFilterProtocol(Message::Protocol protocol);

    // Фильтр по направлению Используем int чтобы хранить
    // "все направления" как -1 без введения лишнего значения в enum
    // -1 = все, иначе сравниваем с Message::Direction
    int  filterDirection() const;
    void setFilterDirection(int direction);

    // Сбросить все фильтры
    void clearFilters();

    // Фильтр по содержимому (поиск по тексту/байтам)
    QString filterText() const;
    void setFilterText(const QString &text);

public slots:
    // Добавить сообщение в лог — подключается к ConnectionManager::messageReceived
    void appendMessage(const Message &message);

    // Очистить весь лог
    void clear();

private:
    // Проверить проходит ли сообщение через текущие фильтры
    bool passesFilter(const Message &message) const;

    // Применить ограничение maxRowCount — удаляет старые строки сверху
    void enforceMaxRowCount();

    // Форматирование для отображения
    static QString formatTimestamp (const QDateTime &dt);
    static QString formatProtocol  (Message::Protocol protocol);
    static QString formatDirection (Message::Direction direction);
    static QString formatPayload   (const QByteArray &payload, bool isText,
                                 int maxLength = 128);
    static QColor  colorForDirection(Message::Direction direction);

    // Все сообщения — отдельно от отображаемых для поддержки фильтрации
    QVector<Message> m_allMessages;

    // Индексы в m_allMessages которые проходят текущий фильтр
    // Модель отображает только эти строки
    QVector<int>     m_filteredIndices;

    int               m_maxRowCount       { 10000 };
    int               m_filterConnectionId{ -1 };
    Message::Protocol m_filterProtocol    { Message::Protocol::Unknown };
    int               m_filterDirection   { -1 };

    // Текст для текстовой фильтрации
    QString m_filterText;
};

#endif // NETSCOPE_MESSAGELOGMODEL_H