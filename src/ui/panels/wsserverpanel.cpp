#include "WsServerPanel.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QLabel>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>

// ---------------------------------------------------------------------------
// WsServerPanel implementation
// ---------------------------------------------------------------------------

WsServerPanel::WsServerPanel(int connectionId,
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

void WsServerPanel::setupUi()
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
    m_portSpin->setValue(9001);     // типичный порт для WebSocket
    m_portSpin->setFixedWidth(80);

    m_startBtn = new QPushButton("Start", m_serverGroup);
    m_stopBtn  = new QPushButton("Stop",  m_serverGroup);
    m_stopBtn->setEnabled(false);

    m_statusLabel = new QLabel("○ Not listening", m_serverGroup);
    m_statusLabel->setStyleSheet("color: #888;");

    // URL-строка для удобного копирования в WsClientPanel
    m_urlLabel = new QLabel(m_serverGroup);
    m_urlLabel->setText("URL: —");
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_urlLabel->setStyleSheet("color: #888; font-family: monospace;");

    // Строка: Address | Port
    QHBoxLayout *addrRow = new QHBoxLayout;
    addrRow->addWidget(new QLabel("Address:", m_serverGroup));
    addrRow->addWidget(m_addressEdit, 1);
    addrRow->addWidget(new QLabel("Port:", m_serverGroup));
    addrRow->addWidget(m_portSpin);

    // Строка: кнопки | статус
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    QVBoxLayout *serverLayout = new QVBoxLayout(m_serverGroup);
    serverLayout->addLayout(addrRow);
    serverLayout->addLayout(btnRow);
    serverLayout->addWidget(m_urlLabel);

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
    m_sendEdit->setPlaceholderText("Enter text message...");

    m_sendBtn = new QPushButton("Send", m_sendGroup);
    m_sendBtn->setFixedWidth(70);
    m_sendBtn->setEnabled(false);

    // Выбор типа WebSocket фрейма — как в WsClientPanel
    m_textRadio   = new QRadioButton("Text frame",   m_sendGroup);
    m_binaryRadio = new QRadioButton("Binary frame", m_sendGroup);
    m_textRadio->setChecked(true);  // по умолчанию text frame

    m_hexCheck       = new QCheckBox("Hex input",               m_sendGroup);
    m_broadcastCheck = new QCheckBox("Broadcast (all clients)", m_sendGroup);

    QHBoxLayout *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendEdit, 1);
    sendRow->addWidget(m_sendBtn);

    QHBoxLayout *frameRow = new QHBoxLayout;
    frameRow->addWidget(m_textRadio);
    frameRow->addWidget(m_binaryRadio);
    frameRow->addSpacing(12);
    frameRow->addWidget(m_hexCheck);
    frameRow->addStretch();

    QHBoxLayout *broadcastRow = new QHBoxLayout;
    broadcastRow->addWidget(m_broadcastCheck);
    broadcastRow->addStretch();

    QVBoxLayout *sendLayout = new QVBoxLayout(m_sendGroup);
    sendLayout->addLayout(sendRow);
    sendLayout->addLayout(frameRow);
    sendLayout->addLayout(broadcastRow);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_serverGroup);
    root->addWidget(m_clientsGroup);
    root->addWidget(m_sendGroup);
    root->addStretch();
}

void WsServerPanel::setupConnections()
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

    // Оба радиобаттона — один слот для обновления placeholder
    connect(m_textRadio,   SIGNAL(toggled(bool)), this, SLOT(onFrameTypeChanged()));
    connect(m_binaryRadio, SIGNAL(toggled(bool)), this, SLOT(onFrameTypeChanged()));

    // Обновление статуса и кнопок при изменении соединения
    connect(m_manager, SIGNAL(connectionInfoChanged(int)),
            this, SLOT(onConnectionInfoChanged(int)));

    // Обновление списка клиентов через сигналы менеджера
    connect(m_manager,
            SIGNAL(wsServerClientConnected(int, int, QString)),
            this,
            SLOT(onWsServerClientConnected(int, int, QString)));

    connect(m_manager,
            SIGNAL(wsServerClientDisconnected(int, int, QString)),
            this,
            SLOT(onWsServerClientDisconnected(int, int, QString)));
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void WsServerPanel::onStartClicked()
{
    const QString addressStr = m_addressEdit->text().trimmed();
    const quint16 port       = static_cast<quint16>(m_portSpin->value());

    QHostAddress address;
    if (addressStr.isEmpty() || addressStr == "0.0.0.0") {
        address = QHostAddress::Any;
    } else {
        address = QHostAddress(addressStr);
        if (address.isNull()) {
            qWarning() << "[WsServerPanel] invalid address:" << addressStr;
            m_statusLabel->setText("✗ Invalid address");
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
    }

    m_manager->startWsServer(m_connectionId, address, port);
}

void WsServerPanel::onStopClicked()
{
    m_manager->stopWsServer(m_connectionId);
}

void WsServerPanel::onSendClicked()
{
    const QString text = m_sendEdit->text();
    if (text.isEmpty()) {
        m_sendEdit->setFocus();
        return;
    }

    const bool isBroadcast = m_broadcastCheck->isChecked();
    const bool isText      = m_textRadio->isChecked();

    if (isBroadcast) {
        // Разослать всем клиентам сервера
        if (isText) {
            m_manager->broadcastWsText(m_connectionId, text);
        } else {
            const QByteArray data = m_hexCheck->isChecked()
            ? parseHex(text) : text.toUtf8();
            if (!data.isEmpty()) {
                m_manager->broadcastWsBinary(m_connectionId, data);
            }
        }
    } else {
        // Отправить конкретному выбранному клиенту
        const int sessionId = selectedSessionId();
        if (sessionId == -1) {
            qWarning() << "[WsServerPanel] no client selected for send";
            return;
        }

        if (isText) {
            m_manager->sendWsTextToSession(m_connectionId, sessionId, text);
        } else {
            const QByteArray data = m_hexCheck->isChecked()
            ? parseHex(text) : text.toUtf8();
            if (!data.isEmpty()) {
                m_manager->sendWsBinaryToSession(m_connectionId, sessionId, data);
            }
        }
    }

    m_sendEdit->clear();
    m_sendEdit->setFocus();
}

void WsServerPanel::onBroadcastToggled(bool checked)
{
    // В режиме broadcast выбор клиента не важен —
    // кнопка Send активна если сервер слушает
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    if (checked) {
        m_sendBtn->setEnabled(info.isActive);
    } else {
        // В режиме unicast — только если выбран клиент
        m_sendBtn->setEnabled(info.isActive && selectedSessionId() != -1);
    }
}

void WsServerPanel::onClientSelectionChanged()
{
    if (m_broadcastCheck->isChecked()) {
        return;     // в broadcast-режиме выбор не влияет на кнопку
    }

    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    m_sendBtn->setEnabled(info.isActive && selectedSessionId() != -1);
}

void WsServerPanel::onFrameTypeChanged()
{
    // Как в WsClientPanel — обновляем placeholder и hex-доступность
    if (m_textRadio->isChecked()) {
        m_sendEdit->setPlaceholderText("Enter text message...");
        m_hexCheck->setChecked(false);
        m_hexCheck->setEnabled(false);
    } else {
        m_sendEdit->setPlaceholderText("Enter binary data (e.g. DE AD BE EF)");
        m_hexCheck->setEnabled(true);
    }
}

void WsServerPanel::onConnectionInfoChanged(int id)
{
    // Фильтруем чужие соединения
    if (id != m_connectionId) {
        return;
    }
    updateUiState();
}

void WsServerPanel::onWsServerClientConnected(int serverId,
                                              int sessionId,
                                              const QString &displayName)
{
    if (serverId != m_connectionId) {
        return;
    }

    QListWidgetItem *item = new QListWidgetItem(
        QString("● %1").arg(displayName), m_clientList
        );
    // Храним sessionId в UserRole для отправки конкретному клиенту
    item->setData(Qt::UserRole, sessionId);
    item->setForeground(QColor(80, 200, 80));

    m_clientsGroup->setTitle(
        QString("Clients (%1 connected)").arg(m_clientList->count())
        );

    // Обновляем кнопку — первый клиент может активировать Send
    onClientSelectionChanged();
}

void WsServerPanel::onWsServerClientDisconnected(int serverId,
                                                 int sessionId,
                                                 const QString &displayName)
{
    Q_UNUSED(displayName)

    if (serverId != m_connectionId) {
        return;
    }

    // Ищем элемент по sessionId в UserRole и удаляем
    for (int i = 0; i < m_clientList->count(); ++i) {
        QListWidgetItem *item = m_clientList->item(i);
        if (item && item->data(Qt::UserRole).toInt() == sessionId) {
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

void WsServerPanel::updateUiState()
{
    const ConnectionInfo info = m_manager->connectionInfo(m_connectionId);
    const bool listening = info.isActive;

    m_startBtn->setEnabled(!listening);
    m_stopBtn->setEnabled(listening);
    m_addressEdit->setEnabled(!listening);
    m_portSpin->setEnabled(!listening);

    if (listening) {
        // Получаем реальный порт из сессий сервера
        const QList<WsClientSession> sessions =
            m_manager->wsServerSessions(m_connectionId);

        // Порт берём из displayName в info — менеджер обновляет его в
        // onWsServerListeningChanged() как "WS Server #N — ws://0.0.0.0:PORT"
        const quint16 actualPort = static_cast<quint16>(m_portSpin->value());

        m_statusLabel->setText(
            QString("● Listening on port %1").arg(actualPort)
            );
        m_statusLabel->setStyleSheet("color: #2a7a2a; font-weight: bold;");

        // Показываем URL для копирования в WsClientPanel
        m_urlLabel->setText(
            QString("URL: ws://127.0.0.1:%1").arg(actualPort)
            );
        m_urlLabel->setStyleSheet(
            "color: #4a9aba; font-family: monospace; font-weight: bold;"
            );

        Q_UNUSED(sessions)
    } else {
        m_statusLabel->setText("○ Not listening");
        m_statusLabel->setStyleSheet("color: #888;");
        m_urlLabel->setText("URL: —");
        m_urlLabel->setStyleSheet("color: #888; font-family: monospace;");
        m_sendBtn->setEnabled(false);
        m_clientList->clear();
        m_clientsGroup->setTitle("Clients (0 connected)");
    }
}

int WsServerPanel::selectedSessionId() const
{
    const QListWidgetItem *item = m_clientList->currentItem();
    if (!item) {
        return -1;
    }
    // sessionId хранится как int в отличие от TcpServerPanel где qintptr
    return item->data(Qt::UserRole).toInt();
}

// static
QByteArray WsServerPanel::parseHex(const QString &hexStr)
{
    // Аналогично WsClientPanel::parseHex() — единый паттерн
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
            qWarning() << "[WsServerPanel] invalid hex byte at position"
                       << i << ":" << hex.mid(i, 2);
        }
    }

    return result;
}