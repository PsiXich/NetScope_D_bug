#ifndef NETSCOPE_UDPENDPOINTPANEL_H
#define NETSCOPE_UDPENDPOINTPANEL_H

#include "core/ConnectionManager.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;

// ---------------------------------------------------------------------------
// UdpEndpointPanel — панель управления одним UDP-эндпоинтом
//
//   - Два независимых блока: Local (bind для приёма) и Remote (цель отправки)
//   - Bind не обязателен для отправки — можно слать без приёма
//   - Broadcast: меняет Remote Address на 255.255.255.255 автоматически
//   - Нет реконнекта и heartbeat — UDP не имеет состояния соединения
// ---------------------------------------------------------------------------
class UdpEndpointPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UdpEndpointPanel(int connectionId,
                              ConnectionManager *manager,
                              QWidget *parent = nullptr);

private slots:
    void onBindClicked();
    void onUnbindClicked();
    void onSendClicked();
    void onBroadcastToggled(bool checked);
    void onConnectionInfoChanged(int id);

private:
    void setupUi();
    void setupConnections();
    void updateUiState();

    // Парсинг hex-строки — тот же паттерн что в остальных панелях
    static QByteArray parseHex(const QString &hexStr);

    int                m_connectionId;
    ConnectionManager *m_manager;       // невладеющий указатель

    // --- Группа Local (Receive) ---
    QGroupBox   *m_localGroup       { nullptr };
    QLineEdit   *m_localAddressEdit { nullptr };
    QSpinBox    *m_localPortSpin    { nullptr };
    QPushButton *m_bindBtn          { nullptr };
    QPushButton *m_unbindBtn        { nullptr };
    QLabel      *m_statusLabel      { nullptr };

    // --- Группа Remote (Send) ---
    QGroupBox   *m_remoteGroup      { nullptr };
    QLineEdit   *m_remoteAddressEdit{ nullptr };
    QSpinBox    *m_remotePortSpin   { nullptr };
    QCheckBox   *m_broadcastCheck   { nullptr };

    // --- Группа Send ---
    QGroupBox   *m_sendGroup        { nullptr };
    QLineEdit   *m_sendEdit         { nullptr };
    QPushButton *m_sendBtn          { nullptr };
    QCheckBox   *m_hexCheck         { nullptr };
};

#endif // NETSCOPE_UDPENDPOINTPANEL_H