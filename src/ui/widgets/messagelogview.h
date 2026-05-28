#ifndef NETSCOPE_MESSAGELOGVIEW_H
#define NETSCOPE_MESSAGELOGVIEW_H

#include "models/MessageLogModel.h"

#include <QWidget>

class QTableView;
class QHeaderView;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;
class QLineEdit;

// ---------------------------------------------------------------------------
// MessageLogView — виджет отображения лога сообщений
//
// Состоит из двух частей:
//   1. Панель фильтров (toolbar сверху):
//      [Connection: All ▼] [Protocol: All ▼] [Direction: All ▼]
//      [☐ Auto-scroll] [Clear]
//
//   2. QTableView подключённый к MessageLogModel
//
// Фильтры управляют MessageLogModel напрямую через setFilter*() методы
// Auto-scroll: при каждом добавлении строки скроллит таблицу вниз —
// удобно при живом трафике Выключается если пользователь вручную
// прокрутил вверх
//
// Двойной клик по строке → сигнал messageDoubleClicked(Message) —
// HexViewer подключится к нему для показа полных данных
// ---------------------------------------------------------------------------
class MessageLogView : public QWidget
{
    Q_OBJECT

public:
    explicit MessageLogView(MessageLogModel *model,
                            QWidget *parent = nullptr);

    // Добавить id соединения в фильтр-комбобокс
    // Вызывается из MainWindow при создании нового соединения
    void addConnectionFilter(int id, const QString &displayName);

    // Удалить соединение из комбобокса фильтра
    // Вызывается из MainWindow при удалении соединения
    void removeConnectionFilter(int id);

signals:
    // Пользователь дважды кликнул по строке лога —
    // HexViewer / детальный просмотр подключается к этому сигналу
    void messageDoubleClicked(const Message &message);

private slots:
    void onConnectionFilterChanged(int index);
    void onProtocolFilterChanged(int index);
    void onDirectionFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void onClearClicked();
    void onRowsInserted();
    void onTableDoubleClicked(const QModelIndex &index);

    // Обновляем счётчик строк в label при каждом изменении модели
    void onRowCountChanged();

private:
    void setupUi();
    void setupConnections();
    void configureTableView();

    // Прокрутить таблицу к последней строке если auto-scroll включён
    void scrollToBottomIfNeeded();

    MessageLogModel *m_model;       // невладеющий указатель

    // --- Фильтр-панель ---
    QComboBox   *m_connectionCombo  { nullptr };
    QComboBox   *m_protocolCombo    { nullptr };
    QComboBox   *m_directionCombo   { nullptr };
    QLineEdit   *m_searchEdit       { nullptr };
    QCheckBox   *m_autoScrollCheck  { nullptr };
    QPushButton *m_clearBtn         { nullptr };
    QLabel      *m_countLabel       { nullptr };

    // --- Таблица ---
    QTableView  *m_tableView        { nullptr };
};

#endif // NETSCOPE_MESSAGELOGVIEW_H
