#ifndef NETSCOPE_TCPCLIENTPANEL_H
#define NETSCOPE_TCPCLIENTPANEL_H

#include "core/ConnectionManager.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;

// ---------------------------------------------------------------------------
// TcpClientPanel — панель управления одним TCP-клиентским соединением.
//
// Панель не хранит состояние соединения — читает его из ConnectionManager
// через connectionInfo() Все действия делегируются менеджеру по id
//
// Обновление UI при изменении состояния:
//   ConnectionManager::connectionInfoChanged(id) → updateUiState()
// ---------------------------------------------------------------------------
class TcpClientPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TcpClientPanel(int connectionId,
                            ConnectionManager *manager,
                            QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onReconnectToggled(bool checked);
    void onConnectionInfoChanged(int id);

private:
    void setupUi();
    void setupConnections();

    // Обновить состояние кнопок и индикатора по текущему статусу соединения
    void updateUiState();

    // Собрать данные для отправки с учётом флага hex-режима
    QByteArray buildSendPayload() const;

    int                m_connectionId;
    ConnectionManager *m_manager;       // невладеющий указатель

    // --- Группа Connection ---
    QGroupBox   *m_connectionGroup  { nullptr };
    QLineEdit   *m_hostEdit         { nullptr };
    QSpinBox    *m_portSpin         { nullptr };
    QPushButton *m_connectBtn       { nullptr };
    QPushButton *m_disconnectBtn    { nullptr };
    QLabel      *m_statusLabel      { nullptr };
    QCheckBox   *m_reconnectCheck   { nullptr };
    QSpinBox    *m_reconnectSpin    { nullptr };

    // --- Группа Send ---
    QGroupBox   *m_sendGroup        { nullptr };
    QLineEdit   *m_sendEdit         { nullptr };
    QPushButton *m_sendBtn          { nullptr };
    QCheckBox   *m_hexCheck         { nullptr };
};

#endif // NETSCOPE_TCPCLIENTPANEL_H
