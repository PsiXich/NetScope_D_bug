#include "MainWindow.h"

#include "ui/panels/TcpClientPanel.h"
#include "ui/panels/TcpServerPanel.h"
#include "ui/panels/WsClientPanel.h"
#include "ui/widgets/MessageLogView.h"

#include <QListView>
#include <QStackedWidget>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QCloseEvent>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>

// ---------------------------------------------------------------------------
// MainWindow implementation
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_manager        (new ConnectionManager(this))
    , m_logModel       (new MessageLogModel(this))
    , m_connectionModel(new ConnectionListModel(m_manager, this))
{
    setWindowTitle(QString("NetScope Debug  v%1").arg(NETSCOPE_VERSION_STR));
    setMinimumSize(900, 600);

    setupUi();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    restoreSettings();

    qDebug() << "[MainWindow] initialized";
}

MainWindow::~MainWindow()
{
    // Инфраструктурные объекты удаляются Qt через parent-child механизм.
    // Явных delete не нужно — все созданы с parent = this.
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // --- Список соединений ---
    m_connectionList = new QListView(this);
    m_connectionList->setModel(m_connectionModel);
    m_connectionList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_connectionList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_connectionList->setFixedWidth(220);
    m_connectionList->setAlternatingRowColors(true);

    // --- Стек панелей ---
    m_panelStack = new QStackedWidget(this);

    // Страница-заглушка когда нет соединений или ни одно не выбрано
    QLabel *placeholder = new QLabel(
        "Select a connection from the list\n"
        "or create a new one using the toolbar.",
        m_panelStack
        );
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #888; font-size: 13px;");
    m_panelStack->addWidget(placeholder);   // index 0 — всегда placeholder

    // --- Лог сообщений ---
    m_logView = new MessageLogView(m_logModel, this);

    // --- Сплиттеры ---
    // Правая часть: панель сверху, лог снизу
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    m_rightSplitter->addWidget(m_panelStack);
    m_rightSplitter->addWidget(m_logView);
    m_rightSplitter->setStretchFactor(0, 2);    // панель получает 2/3
    m_rightSplitter->setStretchFactor(1, 1);    // лог получает 1/3

    // Главный сплиттер: список слева, правая часть справа
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_connectionList);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 0);     // список не растягивается
    m_mainSplitter->setStretchFactor(1, 1);     // правая часть растягивается

    setCentralWidget(m_mainSplitter);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Main");
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_actAddTcpClient = toolBar->addAction("+ TCP Client");
    m_actAddTcpClient->setToolTip("Create a new TCP client connection");

    m_actAddTcpServer = toolBar->addAction("+ TCP Server");
    m_actAddTcpServer->setToolTip("Create a new TCP server");

    m_actAddWsClient = toolBar->addAction("+ WebSocket");
    m_actAddWsClient->setToolTip("Create a new WebSocket client connection");

    toolBar->addSeparator();

    m_actRemove = toolBar->addAction("Remove");
    m_actRemove->setToolTip("Remove selected connection");
    m_actRemove->setEnabled(false);     // активируется при выборе в списке

    toolBar->addSeparator();

    m_actClearLog = toolBar->addAction("Clear Log");
    m_actClearLog->setToolTip("Clear all messages from the log");
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(this);
    m_statusLabel->setText("No connections");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    // --- Toolbar → слоты создания соединений ---
    connect(m_actAddTcpClient, SIGNAL(triggered()),
            this, SLOT(onAddTcpClient()));

    connect(m_actAddTcpServer, SIGNAL(triggered()),
            this, SLOT(onAddTcpServer()));

    connect(m_actAddWsClient, SIGNAL(triggered()),
            this, SLOT(onAddWsClient()));

    connect(m_actRemove, SIGNAL(triggered()),
            this, SLOT(onRemoveConnection()));

    connect(m_actClearLog, SIGNAL(triggered()),
            m_logModel, SLOT(clear()));

    // --- Список соединений → переключение панелей ---
    connect(m_connectionList->selectionModel(),
            SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(onConnectionSelected(QModelIndex)));

    // --- ConnectionManager → MessageLogModel ---
    connect(m_manager, SIGNAL(messageReceived(Message)),
            m_logModel, SLOT(appendMessage(Message)));

    // --- ConnectionManager → обновление статусбара ---
    connect(m_manager, SIGNAL(connectionAdded(ConnectionInfo)),
            this, SLOT(onUpdateStatusBar()));

    connect(m_manager, SIGNAL(connectionRemoved(int)),
            this, SLOT(onUpdateStatusBar()));

    connect(m_manager, SIGNAL(messageReceived(Message)),
            this, SLOT(onUpdateStatusBar()));
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void MainWindow::onAddTcpClient()
{
    const int id = m_manager->createTcpClient();

    // Выбираем новое соединение в списке автоматически
    const int row = m_connectionModel->rowById(id);
    if (row != -1) {
        const QModelIndex idx = m_connectionModel->index(row);
        m_connectionList->setCurrentIndex(idx);
    }

    qDebug() << "[MainWindow] created TcpClient id=" << id;
}

void MainWindow::onAddTcpServer()
{
    const int id = m_manager->createTcpServer();

    const int row = m_connectionModel->rowById(id);
    if (row != -1) {
        const QModelIndex idx = m_connectionModel->index(row);
        m_connectionList->setCurrentIndex(idx);
    }

    qDebug() << "[MainWindow] created TcpServer id=" << id;
}

void MainWindow::onAddWsClient()
{
    const int id = m_manager->createWsClient();

    const int row = m_connectionModel->rowById(id);
    if (row != -1) {
        const QModelIndex idx = m_connectionModel->index(row);
        m_connectionList->setCurrentIndex(idx);
    }

    qDebug() << "[MainWindow] created WsClient id=" << id;
}

void MainWindow::onConnectionSelected(const QModelIndex &index)
{
    if (!index.isValid()) {
        m_panelStack->setCurrentIndex(0);   // показываем placeholder
        m_actRemove->setEnabled(false);
        return;
    }

    const int id = index.data(ConnectionListModel::ConnectionIdRole).toInt();
    m_actRemove->setEnabled(true);

    switchToPanel(id);
}

void MainWindow::onRemoveConnection()
{
    const QModelIndex current = m_connectionList->currentIndex();
    if (!current.isValid()) {
        return;
    }

    const int id = current.data(ConnectionListModel::ConnectionIdRole).toInt();
    const QString name = current.data(ConnectionListModel::DisplayNameRole)
                             .toString();

    // Спрашиваем подтверждение — удаление не отменяется
    const QMessageBox::StandardButton btn = QMessageBox::question(
        this,
        "Remove Connection",
        QString("Remove \"%1\"?\nActive connections will be closed.").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
        );

    if (btn != QMessageBox::Yes) {
        return;
    }

    // Убираем панель из стека если она была создана
    if (m_panelIndices.contains(id)) {
        const int stackIdx = m_panelIndices.value(id);
        QWidget *panel = m_panelStack->widget(stackIdx);
        m_panelStack->removeWidget(panel);
        panel->deleteLater();
        m_panelIndices.remove(id);

        // Пересчитываем индексы оставшихся панелей — после удаления виджета
        // из QStackedWidget индексы последующих виджетов сдвигаются на -1
        for (auto it = m_panelIndices.begin(); it != m_panelIndices.end(); ++it) {
            if (it.value() > stackIdx) {
                it.value() -= 1;
            }
        }
    }

    m_manager->removeConnection(id);
    m_panelStack->setCurrentIndex(0);   // показываем placeholder
    m_actRemove->setEnabled(false);

    onUpdateStatusBar();
}

void MainWindow::onUpdateStatusBar()
{
    const int connCount = m_connectionModel->rowCount();
    const int msgCount  = m_logModel->rowCount();

    m_statusLabel->setText(
        QString("%1 connection%2  |  %3 message%4")
            .arg(connCount)
            .arg(connCount == 1 ? "" : "s")
            .arg(msgCount)
            .arg(msgCount  == 1 ? "" : "s")
        );
}

// ---------------------------------------------------------------------------
// Panel management
// ---------------------------------------------------------------------------

void MainWindow::switchToPanel(int connectionId)
{
    // Панель уже существует — просто переключаемся
    if (m_panelIndices.contains(connectionId)) {
        m_panelStack->setCurrentIndex(m_panelIndices.value(connectionId));
        return;
    }

    // Определяем тип соединения и создаём нужную панель
    QWidget *panel = nullptr;

    if (m_manager->hasTcpClient(connectionId)) {
        panel = new TcpClientPanel(connectionId, m_manager, m_panelStack);
    } else if (m_manager->hasTcpServer(connectionId)) {
        panel = new TcpServerPanel(connectionId, m_manager, m_panelStack);
    } else if (m_manager->hasWsClient(connectionId)) {
        panel = new WsClientPanel(connectionId, m_manager, m_panelStack);
    } else {
        qWarning() << "[MainWindow] switchToPanel: unknown id" << connectionId;
        return;
    }

    const int newIndex = m_panelStack->addWidget(panel);
    m_panelIndices.insert(connectionId, newIndex);
    m_panelStack->setCurrentIndex(newIndex);
}

// ---------------------------------------------------------------------------
// Жизненный цикл окна
// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("mainWindow/geometry",      saveGeometry());
    settings.setValue("mainWindow/mainSplitter",  m_mainSplitter->saveState());
    settings.setValue("mainWindow/rightSplitter", m_rightSplitter->saveState());
}

void MainWindow::restoreSettings()
{
    QSettings settings;

    if (settings.contains("mainWindow/geometry")) {
        restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    }
    if (settings.contains("mainWindow/mainSplitter")) {
        m_mainSplitter->restoreState(
            settings.value("mainWindow/mainSplitter").toByteArray()
            );
    }
    if (settings.contains("mainWindow/rightSplitter")) {
        m_rightSplitter->restoreState(
            settings.value("mainWindow/rightSplitter").toByteArray()
            );
    }
}
