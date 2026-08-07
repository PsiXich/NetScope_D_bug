#ifndef NETSCOPE_WSCLIENTPANEL_H
#define NETSCOPE_WSCLIENTPANEL_H

#include "core/ConnectionManager.h"

#include <QWidget>

class QLineEdit;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QGroupBox;
class QRadioButton;

// ---------------------------------------------------------------------------
// WsClientPanel — панель управления одним WebSocket-соединением.
//
// WebSocket отличается от TCP двумя вещами в UI:
//   1. URL вместо host:port — поддерживает ws:// и wss://
//   2. Явный выбор типа фрейма: Text или Binary
//      Text frame  → sendTextMessage()  → isText = true в Message
//      Binary frame → sendBinaryMessage() → isText = false в Message
// ---------------------------------------------------------------------------
class WsClientPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WsClientPanel(int connectionId,
                           ConnectionManager *manager,
                           QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onReconnectToggled(bool checked);
    void onPingToggled(bool checked);
    void onConnectionInfoChanged(int id);

    // Переключение между Text/Binary — обновляет placeholder в поле ввода
    void onFrameTypeChanged();

private:
    void setupUi();
    void setupConnections();
    void updateUiState();

    // Отправка с учётом выбранного типа фрейма и hex-режима
    void doSend();

    // Парсинг hex-строки в байты — вынесен в утилиту (используется в трёх панелях)
    static QByteArray parseHex(const QString &hexStr);

    int                m_connectionId;
    ConnectionManager *m_manager;

    // --- Группа Connection ---
    QGroupBox   *m_connectionGroup  { nullptr };
    QLineEdit   *m_urlEdit          { nullptr };
    QPushButton *m_connectBtn       { nullptr };
    QPushButton *m_disconnectBtn    { nullptr };
    QLabel      *m_statusLabel      { nullptr };
    QCheckBox   *m_reconnectCheck   { nullptr };
    QSpinBox    *m_reconnectSpin    { nullptr };
    QCheckBox   *m_pingCheck        { nullptr };
    QSpinBox    *m_pingSpin         { nullptr };

    // --- Группа Send ---
    QGroupBox    *m_sendGroup       { nullptr };
    QLineEdit    *m_sendEdit        { nullptr };
    QPushButton  *m_sendBtn         { nullptr };
    QRadioButton *m_textRadio       { nullptr };  // Text frame
    QRadioButton *m_binaryRadio     { nullptr };  // Binary frame
    QCheckBox    *m_hexCheck        { nullptr };  // hex input mode
};

#endif // NETSCOPE_WSCLIENTPANEL_H
