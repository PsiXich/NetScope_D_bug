#ifndef NETSCOPE_TCPSERVERPANEL_H
#define NETSCOPE_TCPSERVERPANEL_H

#include "core/ConnectionManager.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QGroupBox;
class QListWidget;
class QListWidgetItem;
class QCheckBox;

// ---------------------------------------------------------------------------
// TcpServerPanel — панель управления одним TCP-сервером
//
// Список клиентов отображает активные сессии
// Отправка идёт выбранному клиенту или всем (broadcast)
// ---------------------------------------------------------------------------
class TcpServerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TcpServerPanel(int connectionId,
                            ConnectionManager *manager,
                            QWidget *parent = nullptr);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSendClicked();
    void onBroadcastToggled(bool checked);
    void onClientSelectionChanged();

    // Обновление списка клиентов по сигналам менеджера
    void onConnectionInfoChanged(int id);
    void onServerClientConnected(int serverId, qintptr descriptor, const QString &displayName);
    void onServerClientDisconnected(int serverId, qintptr descriptor, const QString &displayName);
    //void onClientConnected(qintptr descriptor, const QString &displayName);
    //void onClientDisconnected(qintptr descriptor, const QString &displayName);

private:
    void setupUi();
    void setupConnections();
    void updateUiState();
    void updateClientList();

    // Получить descriptor выбранного клиента -1 если ничего не выбрано
    qintptr selectedDescriptor() const;

    QByteArray buildSendPayload() const;

    int                m_connectionId;
    ConnectionManager *m_manager;

    // --- Группа Server ---
    QGroupBox   *m_serverGroup      { nullptr };
    QLineEdit   *m_addressEdit      { nullptr };
    QSpinBox    *m_portSpin         { nullptr };
    QPushButton *m_startBtn         { nullptr };
    QPushButton *m_stopBtn          { nullptr };
    QLabel      *m_statusLabel      { nullptr };

    // --- Группа Clients ---
    QGroupBox    *m_clientsGroup    { nullptr };
    QListWidget  *m_clientList      { nullptr };

    // --- Группа Send ---
    QGroupBox   *m_sendGroup        { nullptr };
    QLineEdit   *m_sendEdit         { nullptr };
    QPushButton *m_sendBtn          { nullptr };
    QCheckBox   *m_hexCheck         { nullptr };
    QCheckBox   *m_broadcastCheck   { nullptr };
};

#endif // NETSCOPE_TCPSERVERPANEL_H
