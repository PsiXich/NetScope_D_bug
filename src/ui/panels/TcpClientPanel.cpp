#include "TcpClientPanel.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDebug>

// ---------------------------------------------------------------------------
// TcpClientPanel implementation
// ---------------------------------------------------------------------------

TcpClientPanel::TcpClientPanel(int connectionId,
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

void TcpClientPanel::setupUi()
{
    // -----------------------------------------------------------------------
    // Группа Connection
    // -----------------------------------------------------------------------
    m_connectionGroup = new QGroupBox("Connection", this);

    m_hostEdit = new QLineEdit(m_connectionGroup);
    m_hostEdit->setPlaceholderText("127.0.0.1");
    m_hostEdit->setText("127.0.0.1");

    m_portSpin = new QSpinBox(m_connectionGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    m_portSpin->setFixedWidth(80);

    m_connectBtn    = new QPushButton("Connect",    m_connectionGroup);
    m_disconnectBtn = new QPushButton("Disconnect", m_connectionGroup);
    m_disconnectBtn->setEnabled(false);

    m_statusLabel = new QLabel("● Disconnected", m_connectionGroup);
    m_statusLabel->setStyleSheet("color: #888;");

    m_reconnectCheck = new QCheckBox("Auto-reconnect", m_connectionGroup);

    m_reconnectSpin = new QSpinBox(m_connectionGroup);
    m_reconnectSpin->setRange(500, 60000);
    m_reconnectSpin->setValue(3000);
    m_reconnectSpin->setSuffix(" ms");
    m_reconnectSpin->setFixedWidth(100);
    m_reconnectSpin->setEnabled(false);   // включается вместе с чекбоксом

    // Строка: Host | Port
    QHBoxLayout *hostRow = new QHBoxLayout;
    hostRow->addWidget(new QLabel("Host:", m_connectionGroup));
    hostRow->addWidget(m_hostEdit, 1);
    hostRow->addWidget(new QLabel("Port:", m_connectionGroup));
    hostRow->addWidget(m_portSpin);

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

    QVBoxLayout *connLayout = new QVBoxLayout(m_connectionGroup);
    connLayout->addLayout(hostRow);
    connLayout->addLayout(btnRow);
    connLayout->addLayout(reconnRow);

    // -----------------------------------------------------------------------
    // Группа Send
    // -----------------------------------------------------------------------
    m_sendGroup = new QGroupBox("Send", this);

    m_sendEdit = new QLineEdit(m_sendGroup);
    m_sendEdit->setPlaceholderText("Enter message or hex bytes (e.g. DE AD BE EF)");

    m_sendBtn = new QPushButton("Send", m_sendGroup);
    m_sendBtn->setFixedWidth(70);
    m_sendBtn->setEnabled(false);   // включается когда подключены

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
    root->addWidget(m_connectionGroup);
    root->addWidget(m_sendGroup);
    root->addStretch();     // прижимаем группы к верху
}

void TcpClientPanel::setupConnections()
{
    connect(m_connectBtn, SIGNAL(clicked()),
            this, SLOT(onConnectClicked()));

    connect(m_disconnectBtn, SIGNAL(clicked()),
            this, SLOT(onDisconnectClicked()));

    connect(m_sendBtn, SIGNAL(clicked()),
            this, SLOT(onSendClicked()));

    // Enter в поле отправки — тот же эффект что и кнопка Send
    connect(m_sendEdit, SIGNAL(returnPressed()),
            this, SLOT(onSendClicked()));

    connect(m_reconnectCheck, SIGNAL(toggled(bool)),
            this, SLOT(onReconnectToggled(bool)));

    // Обновляем UI когда менеджер сообщает об изменении нашего соединения
    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void TcpClientPanel::onConnectClicked()
{
    const QString host = m_hostEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(m_portSpin->value());

    if (host.isEmpty()) {
        m_hostEdit->setFocus();
        return;
    }

    // Настраиваем реконнект до подключения
    if (m_reconnectCheck->isChecked()) {
        m_manager->setTcpClientReconnectInterval(
            m_connectionId,
            m_reconnectSpin->value()
            );
    } else {
        m_manager->setTcpClientReconnectInterval(m_connectionId, 0);
    }

    m_manager->connectTcpClient(m_connectionId, host, port);
}

void TcpClientPanel::onDisconnectClicked()
{
    m_manager->disconnectTcpClient(m_connectionId);
}

void TcpClientPanel::onSendClicked()
{
    const QByteArray payload = buildSendPayload();

    if (payload.isEmpty()) {
        m_sendEdit->setFocus();
        return;
    }

    m_manager->sendToTcpClient(m_connectionId, payload);
    m_sendEdit->clear();
    m_sendEdit->setFocus();
}

void TcpClientPanel::onReconnectToggled(bool checked)
{
    m_reconnectSpin->setEnabled(checked);

    // Применяем немедленно если уже подключены
    const int interval = checked ? m_reconnectSpin->value() : 0;
    m_manager->setTcpClientReconnectInterval(m_connectionId, interval);
}

void TcpClientPanel::onConnectionInfoChanged(int id)
{
    // Фильтруем чужие соединения
    if (id != m_connectionId) {
        return;
    }
    updateUiState();
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void TcpClientPanel::updateUiState()
{
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    const bool active = info.isActive;

    // Кнопки
    m_connectBtn->setEnabled(!active);
    m_disconnectBtn->setEnabled(active);
    m_sendBtn->setEnabled(active);

    // Поля ввода — блокируем пока подключены
    m_hostEdit->setEnabled(!active);
    m_portSpin->setEnabled(!active);

    // Статус
    if (active) {
        m_statusLabel->setText("● Connected");
        m_statusLabel->setStyleSheet("color: #2a7a2a; font-weight: bold;");
    } else {
        m_statusLabel->setText("○ Disconnected");
        m_statusLabel->setStyleSheet("color: #888;");
    }
}

QByteArray TcpClientPanel::buildSendPayload() const
{
    const QString text = m_sendEdit->text();

    if (text.isEmpty()) {
        return QByteArray();
    }

    if (!m_hexCheck->isChecked()) {
        // Текстовый режим — просто UTF-8
        return text.toUtf8();
    }

    // Hex режим: парсим "DE AD BE EF" или "DEADBEEF"
    // Убираем пробелы и приводим к верхнему регистру
    const QString hex = QString(text).remove(' ').toUpper();

    if (hex.length() % 2 != 0) {
        qWarning() << "[TcpClientPanel] hex string has odd length, ignoring last nibble";
    }

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
            qWarning() << "[TcpClientPanel] invalid hex byte at position" << i
                       << ":" << hex.mid(i, 2);
        }
    }

    return result;
}