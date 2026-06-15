#ifndef NETSCOPE_WSSERVERPANEL_H
#define NETSCOPE_WSSERVERPANEL_H

#include "core/ConnectionManager.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;
class QListWidget;
class QRadioButton;

// ---------------------------------------------------------------------------
// WsServerPanel — панель управления одним WebSocket-сервером.
// ---------------------------------------------------------------------------
class WsServerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WsServerPanel(int connectionId,
                           ConnectionManager *manager,
                           QWidget *parent = nullptr);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSendClicked();
    void onBroadcastToggled(bool checked);
    void onClientSelectionChanged();
    void onFrameTypeChanged();

    // Обновление по сигналам менеджера
    void onConnectionInfoChanged(int id);
    void onWsServerClientConnected   (int serverId, int sessionId,
                                   const QString &displayName);
    void onWsServerClientDisconnected(int serverId, int sessionId,
                                      const QString &displayName);

private:
    void setupUi();
    void setupConnections();
    void updateUiState();

    // Получить sessionId выбранного клиента. -1 если ничего не выбрано
    int selectedSessionId() const;

    // Парсинг hex-строки — аналогично WsClientPanel
    static QByteArray parseHex(const QString &hexStr);

    int                m_connectionId;
    ConnectionManager *m_manager;       // невладеющий указатель

    // --- Группа Server ---
    QGroupBox   *m_serverGroup      { nullptr };
    QLineEdit   *m_addressEdit      { nullptr };
    QSpinBox    *m_portSpin         { nullptr };
    QPushButton *m_startBtn         { nullptr };
    QPushButton *m_stopBtn          { nullptr };
    QLabel      *m_statusLabel      { nullptr };
    QLabel      *m_urlLabel         { nullptr }; // показывает ws://... для копирования

    // --- Группа Clients ---
    QGroupBox   *m_clientsGroup     { nullptr };
    QListWidget *m_clientList       { nullptr };

    // --- Группа Send ---
    QGroupBox    *m_sendGroup       { nullptr };
    QLineEdit    *m_sendEdit        { nullptr };
    QPushButton  *m_sendBtn         { nullptr };
    QRadioButton *m_textRadio       { nullptr };
    QRadioButton *m_binaryRadio     { nullptr };
    QCheckBox    *m_hexCheck        { nullptr };
    QCheckBox    *m_broadcastCheck  { nullptr };
};

#endif // NETSCOPE_WSSERVERPANEL_H