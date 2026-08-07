#include "WsClientPanel.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUrl>
#include <QDebug>

// ---------------------------------------------------------------------------
// WsClientPanel implementation
// ---------------------------------------------------------------------------

WsClientPanel::WsClientPanel(int connectionId,
                             ConnectionManager *manager,
                             QWidget *parent)
    : QWidget(parent)
    , m_connectionId(connectionId)
    , m_manager(manager)
{
    setupUi();
    setupConnections();
    updateUiState();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void WsClientPanel::setupUi()
{
    // -----------------------------------------------------------------------
    // Группа Connection
    // -----------------------------------------------------------------------
    m_connectionGroup = new QGroupBox("Connection", this);

    m_urlEdit = new QLineEdit(m_connectionGroup);
    m_urlEdit->setPlaceholderText("ws://127.0.0.1:8080/ws");
    m_urlEdit->setText("ws://127.0.0.1:8080/ws");

    m_connectBtn    = new QPushButton("Connect",    m_connectionGroup);
    m_disconnectBtn = new QPushButton("Disconnect", m_connectionGroup);
    m_disconnectBtn->setEnabled(false);

    m_statusLabel = new QLabel("○ Disconnected", m_connectionGroup);
    m_statusLabel->setStyleSheet("color: #888;");

    // --- Реконнект ---
    m_reconnectCheck = new QCheckBox("Auto-reconnect", m_connectionGroup);

    m_reconnectSpin = new QSpinBox(m_connectionGroup);
    m_reconnectSpin->setRange(500, 60000);
    m_reconnectSpin->setValue(3000);
    m_reconnectSpin->setSuffix(" ms");
    m_reconnectSpin->setFixedWidth(100);
    m_reconnectSpin->setEnabled(false);

    // --- Ping ---
    m_pingCheck = new QCheckBox("Ping heartbeat", m_connectionGroup);

    m_pingSpin = new QSpinBox(m_connectionGroup);
    m_pingSpin->setRange(1000, 120000);
    m_pingSpin->setValue(15000);
    m_pingSpin->setSuffix(" ms");
    m_pingSpin->setFixedWidth(100);
    m_pingSpin->setEnabled(false);

    // Строка: URL
    QHBoxLayout *urlRow = new QHBoxLayout;
    urlRow->addWidget(new QLabel("URL:", m_connectionGroup));
    urlRow->addWidget(m_urlEdit, 1);

    // Строка: кнопки | статус
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    // Строка: реконнект
    QHBoxLayout *reconnRow = new QHBoxLayout;
    reconnRow->addWidget(m_reconnectCheck);
    reconnRow->addWidget(new QLabel("Interval:", m_connectionGroup));
    reconnRow->addWidget(m_reconnectSpin);
    reconnRow->addStretch();

    // Строка: ping
    QHBoxLayout *pingRow = new QHBoxLayout;
    pingRow->addWidget(m_pingCheck);
    pingRow->addWidget(new QLabel("Interval:", m_connectionGroup));
    pingRow->addWidget(m_pingSpin);
    pingRow->addStretch();

    QVBoxLayout *connLayout = new QVBoxLayout(m_connectionGroup);
    connLayout->addLayout(urlRow);
    connLayout->addLayout(btnRow);
    connLayout->addLayout(reconnRow);
    connLayout->addLayout(pingRow);

    // -----------------------------------------------------------------------
    // Группа Send
    // -----------------------------------------------------------------------
    m_sendGroup = new QGroupBox("Send", this);

    m_sendEdit = new QLineEdit(m_sendGroup);
    m_sendEdit->setPlaceholderText("Enter text message...");

    m_sendBtn = new QPushButton("Send", m_sendGroup);
    m_sendBtn->setFixedWidth(70);
    m_sendBtn->setEnabled(false);

    m_textRadio   = new QRadioButton("Text frame",   m_sendGroup);
    m_binaryRadio = new QRadioButton("Binary frame", m_sendGroup);
    m_textRadio->setChecked(true);  // по умолчанию — text frame

    m_hexCheck = new QCheckBox("Hex input", m_sendGroup);

    QHBoxLayout *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendEdit, 1);
    sendRow->addWidget(m_sendBtn);

    QHBoxLayout *frameRow = new QHBoxLayout;
    frameRow->addWidget(m_textRadio);
    frameRow->addWidget(m_binaryRadio);
    frameRow->addSpacing(16);
    frameRow->addWidget(m_hexCheck);
    frameRow->addStretch();

    QVBoxLayout *sendLayout = new QVBoxLayout(m_sendGroup);
    sendLayout->addLayout(sendRow);
    sendLayout->addLayout(frameRow);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_connectionGroup);
    root->addWidget(m_sendGroup);
    root->addStretch();
}

void WsClientPanel::setupConnections()
{
    connect(m_connectBtn, SIGNAL(clicked()),
            this, SLOT(onConnectClicked()));

    connect(m_disconnectBtn, SIGNAL(clicked()),
            this, SLOT(onDisconnectClicked()));

    connect(m_sendBtn, SIGNAL(clicked()),
            this, SLOT(onSendClicked()));

    connect(m_sendEdit, SIGNAL(returnPressed()),
            this, SLOT(onSendClicked()));

    connect(m_reconnectCheck, SIGNAL(toggled(bool)),
            this, SLOT(onReconnectToggled(bool)));

    connect(m_pingCheck, SIGNAL(toggled(bool)),
            this, SLOT(onPingToggled(bool)));

    // Оба радиобаттона вызывают один слот — достаточно подключить один,
    // но подключаем оба для явности
    connect(m_textRadio, SIGNAL(toggled(bool)),
            this, SLOT(onFrameTypeChanged()));

    connect(m_binaryRadio, SIGNAL(toggled(bool)),
            this, SLOT(onFrameTypeChanged()));

    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void WsClientPanel::onConnectClicked()
{
    const QString urlStr = m_urlEdit->text().trimmed();
    if (urlStr.isEmpty()) {
        m_urlEdit->setFocus();
        return;
    }

    // fromUserInput добавляет схему "ws://" если она не указана
    QUrl url = QUrl::fromUserInput(urlStr);

    // fromUserInput может добавить "http://" — исправляем на "ws://"
    if (url.scheme() == "http") {
        url.setScheme("ws");
    } else if (url.scheme() == "https") {
        url.setScheme("wss");
    }

    if (!url.isValid()) {
        m_statusLabel->setText("✗ Invalid URL");
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    // Настраиваем параметры до подключения
    const int reconnectMs = m_reconnectCheck->isChecked()
                                ? m_reconnectSpin->value() : 0;
    m_manager->setWsClientReconnectInterval(m_connectionId, reconnectMs);

    const int pingMs = m_pingCheck->isChecked()
                           ? m_pingSpin->value() : 0;
    m_manager->setWsClientPingInterval(m_connectionId, pingMs);

    m_manager->connectWsClient(m_connectionId, url);
}

void WsClientPanel::onDisconnectClicked()
{
    m_manager->disconnectWsClient(m_connectionId);
}

void WsClientPanel::onSendClicked()
{
    doSend();
}

void WsClientPanel::onReconnectToggled(bool checked)
{
    m_reconnectSpin->setEnabled(checked);
    const int ms = checked ? m_reconnectSpin->value() : 0;
    m_manager->setWsClientReconnectInterval(m_connectionId, ms);
}

void WsClientPanel::onPingToggled(bool checked)
{
    m_pingSpin->setEnabled(checked);
    const int ms = checked ? m_pingSpin->value() : 0;
    m_manager->setWsClientPingInterval(m_connectionId, ms);
}

void WsClientPanel::onConnectionInfoChanged(int id)
{
    if (id != m_connectionId) {
        return;
    }
    updateUiState();
}

void WsClientPanel::onFrameTypeChanged()
{
    // Обновляем placeholder чтобы подсказать пользователю что вводить
    if (m_textRadio->isChecked()) {
        m_sendEdit->setPlaceholderText("Enter text message...");
        // В text-режиме hex не имеет смысла — выключаем и скрываем
        m_hexCheck->setChecked(false);
        m_hexCheck->setEnabled(false);
    } else {
        m_sendEdit->setPlaceholderText("Enter binary data (e.g. DE AD BE EF)");
        m_hexCheck->setEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void WsClientPanel::updateUiState()
{
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    const bool active = info.isActive;

    m_connectBtn->setEnabled(!active);
    m_disconnectBtn->setEnabled(active);
    m_sendBtn->setEnabled(active);
    m_urlEdit->setEnabled(!active);

    if (active) {
        m_statusLabel->setText("● Connected");
        m_statusLabel->setStyleSheet("color: #2a7a2a; font-weight: bold;");
    } else {
        m_statusLabel->setText("○ Disconnected");
        m_statusLabel->setStyleSheet("color: #888;");
    }
}

void WsClientPanel::doSend()
{
    const QString text = m_sendEdit->text();
    if (text.isEmpty()) {
        m_sendEdit->setFocus();
        return;
    }

    if (m_textRadio->isChecked()) {
        // Text frame — отправляем как есть
        m_manager->sendWsText(m_connectionId, text);
    } else {
        // Binary frame
        const QByteArray payload = m_hexCheck->isChecked()
                                       ? parseHex(text)
                                       : text.toUtf8();

        if (payload.isEmpty()) {
            qWarning() << "[WsClientPanel] empty payload after parse, not sending";
            return;
        }
        m_manager->sendWsBinary(m_connectionId, payload);
    }

    m_sendEdit->clear();
    m_sendEdit->setFocus();
}

// static
QByteArray WsClientPanel::parseHex(const QString &hexStr)
{
    const QString hex = QString(hexStr).remove(' ').toUpper();

    QByteArray result;
    result.reserve(hex.length() / 2);

    for (int i = 0; i + 1 < hex.length(); i += 2) {
        bool ok = false;
        const uchar byte = static_cast<uchar>(
            hex.mid(i, 2).toUShort(&ok, 16)
            );
        if (ok) {
            result.append(static_cast<char>(byte));
        } else {
            qWarning() << "[WsClientPanel] invalid hex byte at position"
                       << i << ":" << hex.mid(i, 2);
        }
    }

    return result;
}