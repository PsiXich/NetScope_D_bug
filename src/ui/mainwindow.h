#ifndef NETSCOPE_MAINWINDOW_H
#define NETSCOPE_MAINWINDOW_H

#include "core/ConnectionManager.h"
#include "models/MessageLogModel.h"
#include "models/ConnectionListModel.h"

#include <QMainWindow>

class QListView;
class QStackedWidget;
class QSplitter;
class QToolBar;
class QLabel;
class QAction;

class TcpClientPanel;
class TcpServerPanel;
class WsClientPanel;
class MessageLogView;

// ---------------------------------------------------------------------------
// MainWindow — корневой виджет приложения
//
// MainWindow владеет:
//   - ConnectionManager  — центральный реестр соединений
//   - MessageLogModel    — модель лога сообщений
//   - ConnectionListModel — модель списка соединений
//
// Панели (TcpClientPanel и др.) получают ConnectionManager* —
// они не владеют им, только вызывают его методы
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    // Сохраняем геометрию и состояние сплиттеров при закрытии
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Создание новых соединений через toolbar
    void onAddTcpClient();
    void onAddTcpServer();
    void onAddWsClient();

    // Выбор соединения в списке — переключает активную панель
    void onConnectionSelected(const QModelIndex &index);

    // Удаление выбранного соединения
    void onRemoveConnection();

    // Обновление строки статусбара
    void onUpdateStatusBar();

private:
    void setupUi();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();

    // Сохранение / восстановление геометрии окна между запусками
    void saveSettings();
    void restoreSettings();

    // Переключить панель для соединения с данным id
    // Создаёт панель если она ещё не существует
    void switchToPanel(int connectionId);

    // --- Инфраструктура ---
    ConnectionManager    *m_manager         { nullptr };
    MessageLogModel      *m_logModel        { nullptr };
    ConnectionListModel  *m_connectionModel { nullptr };

    // --- Виджеты ---
    QSplitter       *m_mainSplitter     { nullptr }; // горизонтальный: список | панель
    QSplitter       *m_rightSplitter    { nullptr }; // вертикальный: панель | лог
    QListView       *m_connectionList   { nullptr };
    QStackedWidget  *m_panelStack       { nullptr };
    MessageLogView  *m_logView          { nullptr };

    // --- Toolbar actions ---
    QAction *m_actAddTcpClient  { nullptr };
    QAction *m_actAddTcpServer  { nullptr };
    QAction *m_actAddWsClient   { nullptr };
    QAction *m_actRemove        { nullptr };
    QAction *m_actClearLog      { nullptr };

    // --- Статусбар ---
    QLabel  *m_statusLabel      { nullptr };

    // Маппинг connectionId → индекс в m_panelStack
    // Нужен чтобы не создавать панель повторно при повторном выборе
    QMap<int, int>  m_panelIndices;
};

#endif // NETSCOPE_MAINWINDOW_H
