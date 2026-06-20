#include "MessageLogModel.h"

#include <QDateTime>
#include <QTimeZone>
#include <QColor>
#include <QBrush>
#include <QFont>

// ---------------------------------------------------------------------------
// MessageLogModel implementation
// ---------------------------------------------------------------------------

MessageLogModel::MessageLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

// ---------------------------------------------------------------------------
// QAbstractTableModel interface
// ---------------------------------------------------------------------------

int MessageLogModel::rowCount(const QModelIndex &parent) const
{
    // Стандартное требование Qt: для табличных моделей
    // rowCount с валидным parent должен возвращать 0
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredIndices.size();
}

int MessageLogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QVariant MessageLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int row = index.row();
    if (row < 0 || row >= m_filteredIndices.size()) {
        return QVariant();
    }

    const Message &msg = m_allMessages.at(m_filteredIndices.at(row));

    switch (role) {

    // --- Отображаемые данные ---
    case Qt::DisplayRole: {
        switch (index.column()) {
        case ColTime:
            return formatTimestamp(msg.timestamp);
        case ColId:
            return msg.connectionId;
        case ColProtocol:
            return formatProtocol(msg.protocol);
        case ColDirection:
            return formatDirection(msg.direction);
        case ColSize:
            return msg.direction == Message::Direction::System
                       ? QVariant()                    // системные — без размера
                       : QVariant(msg.payload.size());
        case ColData:
            return msg.direction == Message::Direction::System
                       ? msg.info
                       : formatPayload(msg.payload, msg.isText);
        default:
            return QVariant();
        }
    }

    // --- Цвет фона строки по направлению ---
    case Qt::BackgroundRole: {
        const QColor color = colorForDirection(msg.direction);
        // Возвращаем полупрозрачный цвет чтобы не перекрывать
        // системную тему полностью — работает и на тёмных темах
        return QBrush(color);
    }

    // --- Шрифт для колонки Data ---
    case Qt::FontRole: {
        if (index.column() == ColData) {
            QFont font;
            font.setFamily("Courier New");
            font.setPointSize(9);
            return font;
        }
        return QVariant();
    }

    // --- Выравнивание ---
    case Qt::TextAlignmentRole: {
        switch (index.column()) {
        case ColId:
        case ColSize:
            return int(Qt::AlignCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    // --- Tooltip: полные данные без усечения ---
    case Qt::ToolTipRole: {
        if (index.column() == ColData && !msg.payload.isEmpty()) {
            if (msg.isText) {
                return QString::fromUtf8(msg.payload);
            }
            return QString("Binary data: %1 bytes").arg(msg.payload.size());
        }
        return QVariant();
    }

    // --- Кастомные роли для делегатов и тестов ---
    case RawMessageRole:
        return QVariant::fromValue(msg);

    case DirectionRole:
        return QVariant::fromValue(msg.direction);

    case IsTextRole:
        return msg.isText;

    default:
        return QVariant();
    }
}

QVariant MessageLogModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case ColTime:      return QStringLiteral("Time");
    case ColId:        return QStringLiteral("ID");
    case ColProtocol:  return QStringLiteral("Protocol");
    case ColDirection: return QStringLiteral("Direction");
    case ColSize:      return QStringLiteral("Size");
    case ColData:      return QStringLiteral("Data");
    default:           return QVariant();
    }
}

// ---------------------------------------------------------------------------
// Управление данными
// ---------------------------------------------------------------------------

int MessageLogModel::maxRowCount() const
{
    return m_maxRowCount;
}

void MessageLogModel::setMaxRowCount(int count)
{
    m_maxRowCount = (count > 0) ? count : 0;
    enforceMaxRowCount();
}

Message MessageLogModel::messageAt(int row) const
{
    if (row < 0 || row >= m_filteredIndices.size()) {
        return Message();
    }
    return m_allMessages.at(m_filteredIndices.at(row));
}

// ---------------------------------------------------------------------------
// Фильтрация
// ---------------------------------------------------------------------------

int MessageLogModel::filterConnectionId() const
{
    return m_filterConnectionId;
}

void MessageLogModel::setFilterConnectionId(int id)
{
    if (m_filterConnectionId == id) {
        return;
    }
    m_filterConnectionId = id;

    // Полный пересчёт отображаемых индексов
    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allMessages.size(); ++i) {
        if (passesFilter(m_allMessages.at(i))) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

Message::Protocol MessageLogModel::filterProtocol() const
{
    return m_filterProtocol;
}

void MessageLogModel::setFilterProtocol(Message::Protocol protocol)
{
    if (m_filterProtocol == protocol) {
        return;
    }
    m_filterProtocol = protocol;

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allMessages.size(); ++i) {
        if (passesFilter(m_allMessages.at(i))) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

int MessageLogModel::filterDirection() const
{
    return m_filterDirection;
}

void MessageLogModel::setFilterDirection(int direction)
{
    if (m_filterDirection == direction) {
        return;
    }
    m_filterDirection = direction;

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allMessages.size(); ++i) {
        if (passesFilter(m_allMessages.at(i))) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

QString MessageLogModel::filterText() const
{
    return m_filterText;
}

void MessageLogModel::setFilterText(const QString &text)
{
    if(m_filterText == text) {
        return;
    }

    m_filterText = text;

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allMessages.size(); ++i) {
        if (passesFilter(m_allMessages.at(i))) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

void MessageLogModel::clearFilters()
{
    m_filterConnectionId = -1;
    m_filterProtocol     = Message::Protocol::Unknown;
    m_filterDirection    = -1;
    m_filterText.clear();

    beginResetModel();
    m_filteredIndices.clear();
    for (int i = 0; i < m_allMessages.size(); ++i) {
        m_filteredIndices.append(i);
    }
    endResetModel();
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void MessageLogModel::appendMessage(const Message &message)
{
    m_allMessages.append(message);
    const int allIndex = m_allMessages.size() - 1;

    if (passesFilter(message)) {
        // Добавляем строку в конец отображаемого списка
        const int newRow = m_filteredIndices.size();
        beginInsertRows(QModelIndex(), newRow, newRow);
        m_filteredIndices.append(allIndex);
        endInsertRows();
    }

    // Проверяем лимит после вставки
    enforceMaxRowCount();
}

void MessageLogModel::clear()
{
    beginResetModel();
    m_allMessages.clear();
    m_filteredIndices.clear();
    endResetModel();
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

bool MessageLogModel::passesFilter(const Message &message) const
{
    // Фильтр по connectionId
    if (m_filterConnectionId != -1
        && message.connectionId != m_filterConnectionId) {
        return false;
    }

    // Фильтр по протоколу — Unknown означает "показывать все"
    if (m_filterProtocol != Message::Protocol::Unknown
        && message.protocol != m_filterProtocol) {
        return false;
    }

    // Фильтр по направлению — -1 означает "показывать все"
    if (m_filterDirection != -1
        && static_cast<int>(message.direction) != m_filterDirection) {
        return false;
    }

    // Фильтр по тексту payload
    if (!m_filterText.isEmpty()) {
        const QString needle = m_filterText.toLower();

        bool found = false;

        // 1. Всегда ищем в UTF-8 тексте — независимо от isText
        //    TCP-данные могут быть текстом даже при isText=false
        if (!message.payload.isEmpty()) {
            found = QString::fromUtf8(message.payload)
            .toLower()
                .contains(needle);
        }

        // 2. Поиск в info строке (системные события: connect, error и т.д.)
        if (!found && !message.info.isEmpty()) {
            found = message.info.toLower().contains(needle);
        }

        // 3. Поиск в hex-представлении — позволяет искать по "de ad be ef"
        //    или "deadbeef" для бинарных данных
        if (!found && !message.payload.isEmpty()) {
            const QString hexStr = QString::fromLatin1(
                                       message.payload.toHex(' ')  // "68 65 6c 6c 6f" — с пробелами
                                       ).toLower();
            found = hexStr.contains(needle);
        }

        if (!found) return false;
    }

    return true;
}

void MessageLogModel::enforceMaxRowCount()
{
    if (m_maxRowCount <= 0) {
        return;
    }

    // Удаляем строки сверху пока не уложимся в лимит
    // Работаем с m_filteredIndices — пользователь видит только их
    while (m_filteredIndices.size() > m_maxRowCount) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_filteredIndices.removeFirst();
        endRemoveRows();
    }

    // Чистим m_allMessages от записей которые уже не могут попасть
    // в фильтр — держим не более чем 2x maxRowCount чтобы не расти
    // бесконечно при активном фильтре
    const int allLimit = m_maxRowCount * 2;
    if (m_allMessages.size() > allLimit) {
        const int removeCount = m_allMessages.size() - allLimit;

        // Обновляем индексы в m_filteredIndices после удаления из начала
        m_allMessages.remove(0, removeCount);

        for (int i = 0; i < m_filteredIndices.size(); ++i) {
            m_filteredIndices[i] -= removeCount;
        }

        // Удаляем индексы которые стали отрицательными
        // (относились к удалённым сообщениям)
        while (!m_filteredIndices.isEmpty()
               && m_filteredIndices.first() < 0) {
            m_filteredIndices.removeFirst();
        }
    }
}

// ---------------------------------------------------------------------------
// Статические методы форматирования
// ---------------------------------------------------------------------------

QString MessageLogModel::formatTimestamp(const QDateTime &dt)
{
    // Конвертируем UTC → локальное время для отображения
    return dt.toLocalTime().toString("hh:mm:ss.zzz");
}

QString MessageLogModel::formatProtocol(Message::Protocol protocol)
{
    switch (protocol) {
    case Message::Protocol::Tcp:       return QStringLiteral("TCP");
    case Message::Protocol::TcpServer: return QStringLiteral("TCP-S");
    case Message::Protocol::WebSocket: return QStringLiteral("WS");
    case Message::Protocol::WsServer:  return QStringLiteral("WS-S");
    case Message::Protocol::Udp:       return QStringLiteral("UDP");
    case Message::Protocol::Unknown:   return QStringLiteral("?");
    }
    return QStringLiteral("?");
}

QString MessageLogModel::formatDirection(Message::Direction direction)
{
    switch (direction) {
    case Message::Direction::Incoming: return QStringLiteral("↓ In");
    case Message::Direction::Outgoing: return QStringLiteral("↑ Out");
    case Message::Direction::System:   return QStringLiteral("⚙ Sys");
    }
    return QStringLiteral("?");
}

QString MessageLogModel::formatPayload(const QByteArray &payload,
                                       bool isText,
                                       int maxLength)
{
    if (payload.isEmpty()) {
        return QStringLiteral("<empty>");
    }

    if (isText) {
        const QString text = QString::fromUtf8(payload);
        if (text.length() <= maxLength) {
            return text;
        }
        return text.left(maxLength) + QStringLiteral("…");
    }

    // Бинарные данные — hex-превью
    const QByteArray preview = payload.left(maxLength / 3);
    QString hex;
    hex.reserve(preview.size() * 3);

    for (int i = 0; i < preview.size(); ++i) {
        if (i > 0) {
            hex.append(' ');
        }
        hex.append(QString("%1").arg(
                                    static_cast<quint8>(preview.at(i)), 2, 16, QChar('0')
                                    ).toUpper());
    }

    if (payload.size() > preview.size()) {
        hex.append(QStringLiteral(" …"));
    }

    return hex;
}

QColor MessageLogModel::colorForDirection(Message::Direction direction)
{
    switch (direction) {
    case Message::Direction::Incoming:
        // Мягкий зелёный — не перекрывает текст на светлой теме
        return QColor(220, 255, 220, 180);
    case Message::Direction::Outgoing:
        // Мягкий синий
        return QColor(220, 235, 255, 180);
    case Message::Direction::System:
        // Нейтральный серый
        return QColor(245, 245, 245, 180);
    }
    return QColor(Qt::white);
}