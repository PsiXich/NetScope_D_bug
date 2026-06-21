#include "UdpEndpointPanel.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>

// ---------------------------------------------------------------------------
// UdpEndpointPanel implementation
// ---------------------------------------------------------------------------

UdpEndpointPanel::UdpEndpointPanel(int connectionId,
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

void UdpEndpointPanel::setupUi()
{
    // -----------------------------------------------------------------------
    // Группа Local (Receive) — привязка к локальному порту
    // -----------------------------------------------------------------------
    m_localGroup = new QGroupBox("Local (Receive)", this);

    m_localAddressEdit = new QLineEdit(m_localGroup);
    m_localAddressEdit->setPlaceholderText("0.0.0.0  (all interfaces)");
    m_localAddressEdit->setText("0.0.0.0");

    m_localPortSpin = new QSpinBox(m_localGroup);
    m_localPortSpin->setRange(0, 65535);
    m_localPortSpin->setValue(0);
    m_localPortSpin->setSpecialValueText("Auto");   // 0 = ОС выбирает порт
    m_localPortSpin->setFixedWidth(80);

    m_bindBtn   = new QPushButton("Bind",   m_localGroup);
    m_unbindBtn = new QPushButton("Unbind", m_localGroup);
    m_unbindBtn->setEnabled(false);

    m_statusLabel = new QLabel("○ Not bound", m_localGroup);
    m_statusLabel->setStyleSheet("color: #888;");

    QHBoxLayout *localAddrRow = new QHBoxLayout;
    localAddrRow->addWidget(new QLabel("Address:", m_localGroup));
    localAddrRow->addWidget(m_localAddressEdit, 1);
    localAddrRow->addWidget(new QLabel("Port:", m_localGroup));
    localAddrRow->addWidget(m_localPortSpin);

    QHBoxLayout *localBtnRow = new QHBoxLayout;
    localBtnRow->addWidget(m_bindBtn);
    localBtnRow->addWidget(m_unbindBtn);
    localBtnRow->addSpacing(12);
    localBtnRow->addWidget(m_statusLabel);
    localBtnRow->addStretch();

    QVBoxLayout *localLayout = new QVBoxLayout(m_localGroup);
    localLayout->addLayout(localAddrRow);
    localLayout->addLayout(localBtnRow);

    // -----------------------------------------------------------------------
    // Группа Remote (Send) — цель для отправки датаграмм
    // -----------------------------------------------------------------------
    m_remoteGroup = new QGroupBox("Remote (Send)", this);

    m_remoteAddressEdit = new QLineEdit(m_remoteGroup);
    m_remoteAddressEdit->setPlaceholderText("192.168.1.100");

    m_remotePortSpin = new QSpinBox(m_remoteGroup);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(5000);
    m_remotePortSpin->setFixedWidth(80);

    // Broadcast: автоматически заполняет адрес 255.255.255.255
    m_broadcastCheck = new QCheckBox("Broadcast (255.255.255.255)",
                                     m_remoteGroup);

    QHBoxLayout *remoteAddrRow = new QHBoxLayout;
    remoteAddrRow->addWidget(new QLabel("Address:", m_remoteGroup));
    remoteAddrRow->addWidget(m_remoteAddressEdit, 1);
    remoteAddrRow->addWidget(new QLabel("Port:", m_remoteGroup));
    remoteAddrRow->addWidget(m_remotePortSpin);

    QHBoxLayout *remoteBroadcastRow = new QHBoxLayout;
    remoteBroadcastRow->addWidget(m_broadcastCheck);
    remoteBroadcastRow->addStretch();

    QVBoxLayout *remoteLayout = new QVBoxLayout(m_remoteGroup);
    remoteLayout->addLayout(remoteAddrRow);
    remoteLayout->addLayout(remoteBroadcastRow);

    // -----------------------------------------------------------------------
    // Группа Send
    // -----------------------------------------------------------------------
    m_sendGroup = new QGroupBox("Send", this);

    m_sendEdit = new QLineEdit(m_sendGroup);
    m_sendEdit->setPlaceholderText("Enter message or hex bytes");

    m_sendBtn = new QPushButton("Send", m_sendGroup);
    m_sendBtn->setFixedWidth(70);

    m_hexCheck = new QCheckBox("Send as hex", m_sendGroup);

    QHBoxLayout *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendEdit, 1);
    sendRow->addWidget(m_sendBtn);

    QVBoxLayout *sendLayout = new QVBoxLayout(m_sendGroup);
    sendLayout->addLayout(sendRow);
    sendLayout->addWidget(m_hexCheck);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_localGroup);
    root->addWidget(m_remoteGroup);
    root->addWidget(m_sendGroup);
    root->addStretch();
}

void UdpEndpointPanel::setupConnections()
{
    connect(m_bindBtn, SIGNAL(clicked()),
            this, SLOT(onBindClicked()));

    connect(m_unbindBtn, SIGNAL(clicked()),
            this, SLOT(onUnbindClicked()));

    connect(m_sendBtn, SIGNAL(clicked()),
            this, SLOT(onSendClicked()));

    connect(m_sendEdit, SIGNAL(returnPressed()),
            this, SLOT(onSendClicked()));

    connect(m_broadcastCheck, SIGNAL(toggled(bool)),
            this, SLOT(onBroadcastToggled(bool)));

    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void UdpEndpointPanel::onBindClicked()
{
    const QString addressStr = m_localAddressEdit->text().trimmed();
    const quint16 port       = static_cast<quint16>(m_localPortSpin->value());

    QHostAddress address;
    if (addressStr.isEmpty() || addressStr == "0.0.0.0") {
        address = QHostAddress::Any;
    } else {
        address = QHostAddress(addressStr);
        if (address.isNull()) {
            qWarning() << "[UdpEndpointPanel] invalid local address:" << addressStr;
            m_statusLabel->setText("✗ Invalid address");
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
    }

    // Применяем настройки цели отправки перед bind
    const QString remoteAddr = m_remoteAddressEdit->text().trimmed();
    const quint16 remotePort = static_cast<quint16>(m_remotePortSpin->value());

    m_manager->setUdpTarget(m_connectionId, remoteAddr, remotePort);
    m_manager->setUdpBroadcast(m_connectionId, m_broadcastCheck->isChecked());
    m_manager->bindUdpEndpoint(m_connectionId, address, port);
}

void UdpEndpointPanel::onUnbindClicked()
{
    m_manager->unbindUdpEndpoint(m_connectionId);
}

void UdpEndpointPanel::onSendClicked()
{
    const QString text = m_sendEdit->text();
    if (text.isEmpty()) {
        m_sendEdit->setFocus();
        return;
    }

    // Обновляем цель отправки из UI перед каждой отправкой —
    // пользователь может изменить адрес без повторного bind
    const QString remoteAddr = m_remoteAddressEdit->text().trimmed();
    const quint16 remotePort = static_cast<quint16>(m_remotePortSpin->value());

    if (remoteAddr.isEmpty()) {
        qWarning() << "[UdpEndpointPanel] remote address not set";
        m_remoteAddressEdit->setFocus();
        return;
    }

    m_manager->setUdpTarget(m_connectionId, remoteAddr, remotePort);

    const QByteArray payload = m_hexCheck->isChecked()
                                   ? parseHex(text)
                                   : text.toUtf8();

    if (payload.isEmpty()) {
        return;
    }

    m_manager->sendUdpData(m_connectionId, payload);

    m_sendEdit->clear();
    m_sendEdit->setFocus();
}

void UdpEndpointPanel::onBroadcastToggled(bool checked)
{
    if (checked) {
        // Заполняем адрес broadcast автоматически
        m_remoteAddressEdit->setText("255.255.255.255");
        m_remoteAddressEdit->setEnabled(false);
    } else {
        m_remoteAddressEdit->clear();
        m_remoteAddressEdit->setEnabled(true);
        m_remoteAddressEdit->setFocus();
    }

    m_manager->setUdpBroadcast(m_connectionId, checked);
}

void UdpEndpointPanel::onConnectionInfoChanged(int id)
{
    if (id != m_connectionId) {
        return;
    }
    updateUiState();
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void UdpEndpointPanel::updateUiState()
{
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    const bool bound = info.isActive;

    // Кнопки Bind/Unbind
    m_bindBtn->setEnabled(!bound);
    m_unbindBtn->setEnabled(bound);

    // Локальные настройки блокируем пока привязаны
    m_localAddressEdit->setEnabled(!bound);
    m_localPortSpin->setEnabled(!bound);

    // Статус
    if (bound) {
        m_statusLabel->setText("● Bound");
        m_statusLabel->setStyleSheet("color: #2a7a2a; font-weight: bold;");
    } else {
        m_statusLabel->setText("○ Not bound");
        m_statusLabel->setStyleSheet("color: #888;");
    }

    // Remote и Send доступны всегда — можно слать без bind
    // (Send без bind не создаёт постоянный локальный порт,
    //  ОС выбирает эфемерный порт для каждой датаграммы)
}

// static
QByteArray UdpEndpointPanel::parseHex(const QString &hexStr)
{
    // Аналогично остальным панелям — единый паттерн по проекту
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
            qWarning() << "[UdpEndpointPanel] invalid hex byte at position"
                       << i << ":" << hex.mid(i, 2);
        }
    }

    return result;
}