#ifndef NETSCOPE_CONNECTIONLISTMODEL_H
#define NETSCOPE_CONNECTIONLISTMODEL_H

#include "core/ConnectionManager.h"

#include <QAbstractListModel>
#include <QVector>

// ---------------------------------------------------------------------------
// ConnectionListModel — список всех соединений для боковой панели UI
//
// Наследуем QAbstractListModel — данные одномерны
// QListView / QComboBox подключаются напрямую без дополнительной обёртки
//
// Модель подписывается на три сигнала ConnectionManager:
//   connectionAdded()       → добавить строку
//   connectionRemoved()     → удалить строку
//   connectionInfoChanged() → обновить строку (dataChanged)
// ---------------------------------------------------------------------------
class ConnectionListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum CustomRole {
        ConnectionIdRole    = Qt::UserRole + 1, // int: id соединения
        ConnectionTypeRole  = Qt::UserRole + 2, // ConnectionInfo::Type
        IsActiveRole        = Qt::UserRole + 3, // bool: активно ли соединение
        DisplayNameRole     = Qt::UserRole + 4  // QString: полное имя
    };

    explicit ConnectionListModel(ConnectionManager *manager,
                                 QObject *parent = nullptr);

    // --- QAbstractListModel interface ---
    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data    (const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    // --- Вспомогательные методы для UI ---

    // Найти строку по id соединения Возвращает -1 если не найден
    int rowById(int connectionId) const;

    // Получить ConnectionInfo по строке модели
    ConnectionInfo infoAt(int row) const;

public slots:
    void onConnectionAdded  (const ConnectionInfo &info);
    void onConnectionRemoved(int id);
    void onConnectionInfoChanged(int id);

private:
    // Найти индекс в m_connections по id Возвращает -1 если не найден
    int indexById(int id) const;

    // Иконка / префикс для типа соединения
    static QString typePrefix(ConnectionInfo::Type type);

    // ConnectionManager не владеем — он живёт дольше модели
    ConnectionManager       *m_manager;
    QVector<ConnectionInfo>  m_connections;
};

#endif // NETSCOPE_CONNECTIONLISTMODEL_H
