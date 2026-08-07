#include "TcpServerPanel.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>

// ---------------------------------------------------------------------------
// TcpServerPanel implementation
// ---------------------------------------------------------------------------

TcpServerPanel::TcpServerPanel(int connectionId,
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

void TcpServerPanel::setupUi()
{
    // -----------------------------------------------------------------------
    // Группа Server
    // -----------------------------------------------------------------------
    m_serverGroup = new QGroupBox("Server", this);

    m_addressEdit = new QLineEdit(m_serverGroup);
    m_addressEdit->setPlaceholderText("0.0.0.0  (all interfaces)");
    m_addressEdit->setText("0.0.0.0");

    m_portSpin = new QSpinBox(m_serverGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(9000);
    m_portSpin->setFixedWidth(80);

    m_startBtn = new QPushButton("Start",  m_serverGroup);
    m_stopBtn  = new QPushButton("Stop",   m_serverGroup);
    m_stopBtn->setEnabled(false);

    m_statusLabel = new QLabel("○ Not listening", m_serverGroup);
    m_statusLabel->setStyleSheet("color: #888;");

    QHBoxLayout *addrRow = new QHBoxLayout;
    addrRow->addWidget(new QLabel("Address:", m_serverGroup));
    addrRow->addWidget(m_addressEdit, 1);
    addrRow->addWidget(new QLabel("Port:", m_serverGroup));
    addrRow->addWidget(m_portSpin);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    QVBoxLayout *serverLayout = new QVBoxLayout(m_serverGroup);
    serverLayout->addLayout(addrRow);
    serverLayout->addLayout(btnRow);

    // -----------------------------------------------------------------------
    // Группа Clients
    // -----------------------------------------------------------------------
    m_clientsGroup = new QGroupBox("Clients (0 connected)", this);

    m_clientList = new QListWidget(m_clientsGroup);
    m_clientList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_clientList->setAlternatingRowColors(true);
    m_clientList->setFixedHeight(120);

    QVBoxLayout *clientsLayout = new QVBoxLayout(m_clientsGroup);
    clientsLayout->addWidget(m_clientList);

    // -----------------------------------------------------------------------
    // Группа Send
    // -----------------------------------------------------------------------
    m_sendGroup = new QGroupBox("Send", this);

    m_sendEdit = new QLineEdit(m_sendGroup);
    m_sendEdit->setPlaceholderText("Enter message or hex bytes");

    m_sendBtn = new QPushButton("Send", m_sendGroup);
    m_sendBtn->setFixedWidth(70);
    m_sendBtn->setEnabled(false);

    m_hexCheck       = new QCheckBox("Send as hex",              m_sendGroup);
    m_broadcastCheck = new QCheckBox("Broadcast (all clients)",  m_sendGroup);

    QHBoxLayout *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendEdit, 1);
    sendRow->addWidget(m_sendBtn);

    QHBoxLayout *optRow = new QHBoxLayout;
    optRow->addWidget(m_hexCheck);
    optRow->addSpacing(16);
    optRow->addWidget(m_broadcastCheck);
    optRow->addStretch();

    QVBoxLayout *sendLayout = new QVBoxLayout(m_sendGroup);
    sendLayout->addLayout(sendRow);
    sendLayout->addLayout(optRow);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_serverGroup);
    root->addWidget(m_clientsGroup);
    root->addWidget(m_sendGroup);
    root->addStretch();
}

void TcpServerPanel::setupConnections()
{
    connect(m_startBtn, SIGNAL(clicked()),
            this, SLOT(onStartClicked()));

    connect(m_stopBtn, SIGNAL(clicked()),
            this, SLOT(onStopClicked()));

    connect(m_sendBtn, SIGNAL(clicked()),
            this, SLOT(onSendClicked()));

    connect(m_sendEdit, SIGNAL(returnPressed()),
            this, SLOT(onSendClicked()));

    connect(m_broadcastCheck, SIGNAL(toggled(bool)),
            this, SLOT(onBroadcastToggled(bool)));

    connect(m_clientList, SIGNAL(itemSelectionChanged()),
            this, SLOT(onClientSelectionChanged()));

    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));

    connect(m_manager,
            SIGNAL(serverClientConnected(int, qintptr, QString)),
            this,
            SLOT(onServerClientConnected(int, qintptr, QString)));

    connect(m_manager,
            SIGNAL(serverClientDisconnected(int, qintptr, QString)),
            this,
            SLOT(onServerClientDisconnected(int, qintptr, QString)));
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void TcpServerPanel::onStartClicked()
{
    const QString addressStr = m_addressEdit->text().trimmed();
    const quint16 port       = static_cast<quint16>(m_portSpin->value());

    QHostAddress address;
    if (addressStr.isEmpty() || addressStr == "0.0.0.0") {
        address = QHostAddress::Any;
    } else {
        address = QHostAddress(addressStr);
        if (address.isNull()) {
            qWarning() << "[TcpServerPanel] invalid address:" << addressStr;
            m_statusLabel->setText("✗ Invalid address");
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
    }

    m_manager->startTcpServer(m_connectionId, address, port);
}

void TcpServerPanel::onStopClicked()
{
    m_manager->stopTcpServer(m_connectionId);
}

void TcpServerPanel::onSendClicked()
{
    const QByteArray payload = buildSendPayload();
    if (payload.isEmpty()) {
        m_sendEdit->setFocus();
        return;
    }

    if (m_broadcastCheck->isChecked()) {
        m_manager->broadcastTcpServer(m_connectionId, payload);
    } else {
        const qintptr descriptor = selectedDescriptor();
        if (descriptor == -1) {
            qWarning() << "[TcpServerPanel] no client selected for send";
            return;
        }
        m_manager->sendToTcpServerClient(m_connectionId, descriptor, payload);
    }

    m_sendEdit->clear();
    m_sendEdit->setFocus();
}

void TcpServerPanel::onBroadcastToggled(bool checked)
{
    // В режиме broadcast выбор клиента не важен —
    // кнопка Send активна если сервер слушает
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    if (checked) {
        m_sendBtn->setEnabled(info.isActive);
    } else {
        // В режиме unicast — только если выбран клиент
        m_sendBtn->setEnabled(info.isActive && selectedDescriptor() != -1);
    }
}

void TcpServerPanel::onClientSelectionChanged()
{
    if (m_broadcastCheck->isChecked()) {
        return;     // в broadcast-режиме выбор не влияет на кнопку
    }

    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    m_sendBtn->setEnabled(info.isActive && selectedDescriptor() != -1);
}

void TcpServerPanel::onConnectionInfoChanged(int id)
{
    if (id != m_connectionId) {
        return;
    }
    updateUiState();
}

void TcpServerPanel::onServerClientConnected(int serverId,
                                             qintptr descriptor,
                                             const QString &displayName)
{
    if (serverId != m_connectionId) return;

    QListWidgetItem *item = new QListWidgetItem(
        QString("● %1").arg(displayName), m_clientList
        );
    // Храним descriptor в UserRole для отправки конкретному клиенту
    item->setData(Qt::UserRole, static_cast<qint64>(descriptor));
    item->setForeground(QColor(80, 200, 80));

    m_clientsGroup->setTitle(
        QString("Clients (%1 connected)").arg(m_clientList->count())
        );

    // Обновляем кнопку Send — клиент появился
    onClientSelectionChanged();
}

void TcpServerPanel::onServerClientDisconnected(int serverId,
                                                qintptr descriptor,
                                                const QString &displayName)
{
    Q_UNUSED(displayName)
    if (serverId != m_connectionId) return;

    // Ищем элемент по descriptor в UserRole
    for (int i = 0; i < m_clientList->count(); ++i) {
        QListWidgetItem *item = m_clientList->item(i);
        if (item && item->data(Qt::UserRole).toLongLong()
                        == static_cast<qint64>(descriptor)) {
            delete m_clientList->takeItem(i);
            break;
        }
    }

    m_clientsGroup->setTitle(
        QString("Clients (%1 connected)").arg(m_clientList->count())
        );

    onClientSelectionChanged();
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void TcpServerPanel::updateUiState()
{
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    const bool listening = info.isActive;

    m_startBtn->setEnabled(!listening);
    m_stopBtn->setEnabled(listening);
    m_addressEdit->setEnabled(!listening);
    m_portSpin->setEnabled(!listening);

    if (listening) {
        m_statusLabel->setText(
            QString("● Listening on port %1")
                .arg(m_portSpin->value())
            );
        m_statusLabel->setStyleSheet("color: #2a7a2a; font-weight: bold;");
    } else {
        m_statusLabel->setText("○ Not listening");
        m_statusLabel->setStyleSheet("color: #888;");
        m_sendBtn->setEnabled(false);
        m_clientList->clear();
        m_clientsGroup->setTitle("Clients (0 connected)");
    }
}

qintptr TcpServerPanel::selectedDescriptor() const
{
    const QListWidgetItem *item = m_clientList->currentItem();
    if (!item) {
        return -1;
    }

    // descriptor хранится в Qt::UserRole как qint64 —
    // qintptr может быть 32 или 64 бит в зависимости от платформы
    return static_cast<qintptr>(item->data(Qt::UserRole).toLongLong());
}

QByteArray TcpServerPanel::buildSendPayload() const
{
    const QString text = m_sendEdit->text();
    if (text.isEmpty()) {
        return QByteArray();
    }

    if (!m_hexCheck->isChecked()) {
        return text.toUtf8();
    }

    // Hex парсинг — идентичен TcpClientPanel::buildSendPayload()
    const QString hex = QString(text).remove(' ').toUpper();
    QByteArray result;
    result.reserve(hex.length() / 2);

    for (int i = 0; i + 1 < hex.length(); i += 2) {
        bool ok = false;
        const uchar byte = static_cast<uchar>(
            hex.mid(i, 2).toUShort(&ok, 16)
            );
        if (ok) {
            result.append(static_cast<char>(byte));
        }
    }

    return result;
}