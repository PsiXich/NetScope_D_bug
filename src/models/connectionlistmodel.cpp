#include "ConnectionListModel.h"

#include <QFont>
#include <QBrush>
#include <QColor>

// ---------------------------------------------------------------------------
// ConnectionListModel implementation
// ---------------------------------------------------------------------------

ConnectionListModel::ConnectionListModel(ConnectionManager *manager,
                                         QObject *parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
{
    Q_ASSERT_X(manager != nullptr,
               "ConnectionListModel",
               "ConnectionManager pointer must not be null");

    // Подписываемся на сигналы менеджера.
    // Используем старый синтаксис для единообразия с остальным кодом Qt 5.7.
    connect(m_manager, SIGNAL(connectionAdded(ConnectionInfo)),
            this, SLOT(onConnectionAdded(ConnectionInfo)));

    connect(m_manager, SIGNAL(connectionRemoved(int)),
            this, SLOT(onConnectionRemoved(int)));

    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));

    // Синхронизируем начальное состояние — менеджер мог уже
    // содержать соединения если модель создана позже него
    const QList<ConnectionInfo> existing = m_manager->connections();
    for (const ConnectionInfo &info : existing) {
        m_connections.append(info);
    }
}

// ---------------------------------------------------------------------------
// QAbstractListModel interface
// ---------------------------------------------------------------------------

int ConnectionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_connections.size();
}

QVariant ConnectionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int row = index.row();
    if (row < 0 || row >= m_connections.size()) {
        return QVariant();
    }

    const ConnectionInfo &info = m_connections.at(row);

    switch (role) {

    // Основная строка отображения: "[TCP] My Connection — Connected"
    case Qt::DisplayRole: {
        const QString status = info.isActive
                                   ? QStringLiteral("● Connected")
                                   : QStringLiteral("○ Idle");
        return QString("%1 %2  %3")
            .arg(typePrefix(info.type))
            .arg(info.displayName)
            .arg(status);
    }

    // Tooltip — полное имя без усечения
    case Qt::ToolTipRole:
        return info.displayName;

    // Цвет текста: зелёный если активно, серый если нет
    case Qt::ForegroundRole: {
        if (info.isActive) {
            return QBrush(QColor(34, 139, 34));    // тёмно-зелёный
        }
        return QBrush(QColor(120, 120, 120));       // серый
    }

    // Жирный шрифт для активных соединений
    case Qt::FontRole: {
        QFont font;
        font.setBold(info.isActive);
        return font;
    }

    // --- Кастомные роли ---
    case ConnectionIdRole:
        return info.id;

    case ConnectionTypeRole:
        return QVariant::fromValue(info.type);

    case IsActiveRole:
        return info.isActive;

    case DisplayNameRole:
        return info.displayName;

    default:
        return QVariant();
    }
}

// ---------------------------------------------------------------------------
// Вспомогательные методы
// ---------------------------------------------------------------------------

int ConnectionListModel::rowById(int connectionId) const
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections.at(i).id == connectionId) {
            return i;
        }
    }
    return -1;
}

ConnectionInfo ConnectionListModel::infoAt(int row) const
{
    if (row < 0 || row >= m_connections.size()) {
        return ConnectionInfo();
    }
    return m_connections.at(row);
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void ConnectionListModel::onConnectionAdded(const ConnectionInfo &info)
{
    const int newRow = m_connections.size();

    beginInsertRows(QModelIndex(), newRow, newRow);
    m_connections.append(info);
    endInsertRows();
}

void ConnectionListModel::onConnectionRemoved(int id)
{
    const int row = indexById(id);
    if (row == -1) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_connections.remove(row);
    endRemoveRows();
}

void ConnectionListModel::onConnectionInfoChanged(int id)
{
    const int row = indexById(id);
    if (row == -1) {
        return;
    }

    // Синхронизируем данные из менеджера
    m_connections[row] = m_manager->connectionInfo(id);

    // Уведомляем view об изменении только этой строки —
    // не сбрасываем всю модель
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

int ConnectionListModel::indexById(int id) const
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

QString ConnectionListModel::typePrefix(ConnectionInfo::Type type)
{
    switch (type) {
    case ConnectionInfo::Type::TcpClient:  return QStringLiteral("[TCP]");
    case ConnectionInfo::Type::TcpServer:  return QStringLiteral("[SRV]");
    case ConnectionInfo::Type::WebSocket:  return QStringLiteral("[WS] ");
    case ConnectionInfo::Type::WsServer:   return QStringLiteral("[WSS]");
    }
    return QStringLiteral("[?]  ");
}