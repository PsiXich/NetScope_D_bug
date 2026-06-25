#include "MessageLogView.h"

#include <QTableView>
#include <QHeaderView>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QDebug>
#include <QLineEdit>

// ---------------------------------------------------------------------------
// MessageLogView implementation
// ---------------------------------------------------------------------------

MessageLogView::MessageLogView(MessageLogModel *model, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    setupConnections();
    configureTableView();
}

// ---------------------------------------------------------------------------
// Публичные методы
// ---------------------------------------------------------------------------

void MessageLogView::addConnectionFilter(int id, const QString &displayName)
{
    // Данные: id хранится в UserRole — извлекаем в onConnectionFilterChanged()
    m_connectionCombo->addItem(displayName, id);
}

void MessageLogView::removeConnectionFilter(int id)
{
    // Ищем элемент с соответствующим ID в UserRole и удаляем его из списка
    for (int i = 0; i < m_connectionCombo->count(); ++i) {
        if (m_connectionCombo->itemData(i).toInt() == id) {
            m_connectionCombo->removeItem(i);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void MessageLogView::setupUi()
{
    // -----------------------------------------------------------------------
    // Панель фильтров
    // -----------------------------------------------------------------------

    // --- Фильтр по соединению ---
    m_connectionCombo = new QComboBox(this);
    m_connectionCombo->addItem("All connections", -1);  // -1 = без фильтра
    m_connectionCombo->setMinimumWidth(140);

    // --- Фильтр по протоколу ---
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem("All protocols",
                             static_cast<int>(Message::Protocol::Unknown));
    m_protocolCombo->addItem("TCP",
                             static_cast<int>(Message::Protocol::Tcp));
    m_protocolCombo->addItem("TCP Server",
                             static_cast<int>(Message::Protocol::TcpServer));
    m_protocolCombo->addItem("WebSocket",
                             static_cast<int>(Message::Protocol::WebSocket));
    m_protocolCombo->addItem("WS Server",
                             static_cast<int>(Message::Protocol::WsServer));
    m_protocolCombo->addItem("UDP",
                             static_cast<int>(Message::Protocol::Udp));
    m_protocolCombo->addItem("MQTT",
                             static_cast<int>(Message::Protocol::Mqtt));

    // --- Фильтр по направлению ---
    m_directionCombo = new QComboBox(this);
    m_directionCombo->addItem("All directions", -1);
    m_directionCombo->addItem("↓ Incoming",
                              static_cast<int>(Message::Direction::Incoming));
    m_directionCombo->addItem("↑ Outgoing",
                              static_cast<int>(Message::Direction::Outgoing));
    m_directionCombo->addItem("⚙ System",
                              static_cast<int>(Message::Direction::System));

    m_autoScrollCheck = new QCheckBox("Auto-scroll", this);
    m_autoScrollCheck->setChecked(true);

    // --- Поиск по тексту (payload) ---
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search payload...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(150);

    // --- Поиск по Topic ---
    m_topicFilterEdit = new QLineEdit(this);
    m_topicFilterEdit->setPlaceholderText("Filter by MQTT topic...");
    m_topicFilterEdit->setClearButtonEnabled(true);
    m_topicFilterEdit->setToolTip(
        "Filter messages by MQTT topic.\n"
        "Supports MQTT wildcards:\n"
        "  #  — matches any number of levels  (home/#)\n"
        "  +  — matches exactly one level     (sensor/+/temp)\n\n"
        "Non-MQTT messages are not affected by this filter."
        );

    // --- Управление логом ---
    m_clearBtn = new QPushButton("Clear", this);
    m_clearBtn->setFixedWidth(60);

    m_countLabel = new QLabel("0 messages", this);
    m_countLabel->setStyleSheet("color: #666;");
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // --- Компоновка панели фильтров ---
    // 1. Верхний ряд: Настройки соединения и основные фильтры
    QHBoxLayout *topFilterRow = new QHBoxLayout;
    topFilterRow->addWidget(new QLabel("Connection:", this));
    topFilterRow->addWidget(m_connectionCombo);
    topFilterRow->addSpacing(8);

    topFilterRow->addWidget(new QLabel("Protocol:", this));
    topFilterRow->addWidget(m_protocolCombo);
    topFilterRow->addSpacing(8);

    topFilterRow->addWidget(new QLabel("Direction:", this));
    topFilterRow->addWidget(m_directionCombo);

    topFilterRow->addStretch(); // Прижимает всё влево

    // 2. Нижний ряд: Поиск, автоскролл и счетчик
    QHBoxLayout *bottomFilterRow = new QHBoxLayout;
    bottomFilterRow->addWidget(new QLabel("Topic:", this));
    bottomFilterRow->addWidget(m_topicFilterEdit);
    bottomFilterRow->addSpacing(16);

    bottomFilterRow->addWidget(new QLabel("Text:", this));
    bottomFilterRow->addWidget(m_searchEdit);
    bottomFilterRow->addSpacing(16);

    bottomFilterRow->addWidget(m_autoScrollCheck);
    bottomFilterRow->addSpacing(8);
    bottomFilterRow->addWidget(m_clearBtn);

    bottomFilterRow->addStretch(); // Прижимает кнопки влево, а счетчик вправо
    bottomFilterRow->addWidget(m_countLabel);

    // 3. Объединяем оба ряда
    QVBoxLayout *filterLayout = new QVBoxLayout;
    filterLayout->addLayout(topFilterRow);
    filterLayout->addLayout(bottomFilterRow);

    // -----------------------------------------------------------------------
    // Таблица
    // -----------------------------------------------------------------------
    m_tableView = new QTableView(this);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 4, 0, 0);
    root->addLayout(filterLayout);
    root->addWidget(m_tableView, 1);    // таблица растягивается
}

void MessageLogView::setupConnections()
{
    connect(m_connectionCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onConnectionFilterChanged(int)));

    connect(m_protocolCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onProtocolFilterChanged(int)));

    connect(m_directionCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onDirectionFilterChanged(int)));

    connect(m_searchEdit, SIGNAL(textChanged(QString)),
            this, SLOT(onSearchTextChanged(QString)));

    connect(m_topicFilterEdit, SIGNAL(textChanged(QString)),
            this, SLOT(onTopicFilterChanged(QString)));

    connect(m_clearBtn, SIGNAL(clicked()),
            this, SLOT(onClearClicked()));

    // --- Сигналы от модели данных ---
    // Следим за вставкой строк для auto-scroll и счётчика
    connect(m_model, SIGNAL(rowsInserted(QModelIndex, int, int)),
            this, SLOT(onRowsInserted()));

    // Следим за любыми изменениями количества строк для счётчика
    connect(m_model, SIGNAL(rowsRemoved(QModelIndex, int, int)),
            this, SLOT(onRowCountChanged()));

    connect(m_model, SIGNAL(modelReset()),
            this, SLOT(onRowCountChanged()));

    // --- Интерактивность таблицы ---
    // Двойной клик по строке
    connect(m_tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(onTableDoubleClicked(QModelIndex)));
}

void MessageLogView::configureTableView()
{
    m_tableView->setModel(m_model);

    // --- Поведение выделения ---
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // --- Внешний вид ---
    m_tableView->setAlternatingRowColors(false); // цвет задаёт модель через BackgroundRole
    m_tableView->setShowGrid(false);
    m_tableView->setWordWrap(false);             // без переноса — фиксированная высота строки
    m_tableView->verticalHeader()->hide();       // номера строк не нужны
    m_tableView->setFont(QFont("Courier New", 9));

    // --- Ширины колонок ---
    QHeaderView *hdr = m_tableView->horizontalHeader();
    hdr->setStretchLastSection(true);   // колонка Data занимает остаток

    m_tableView->setColumnWidth(MessageLogModel::ColTime,      90);
    m_tableView->setColumnWidth(MessageLogModel::ColId,        35);
    m_tableView->setColumnWidth(MessageLogModel::ColProtocol,  72);
    m_tableView->setColumnWidth(MessageLogModel::ColDirection, 72);
    m_tableView->setColumnWidth(MessageLogModel::ColSize,      50);
    // ColData — stretch, ширина не задаётся

    // --- Высота строк ---
    // Фиксированная высота вместо resizeRowsToContents() —
    // для тысяч строк это критично для производительности
    m_tableView->verticalHeader()->setDefaultSectionSize(22);
    m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // --- Сортировка ---
    // Не включаем — лог должен оставаться в хронологическом порядке
    m_tableView->setSortingEnabled(false);
}

// ---------------------------------------------------------------------------
// Слоты
// ---------------------------------------------------------------------------

void MessageLogView::onConnectionFilterChanged(int index)
{
    const int id = m_connectionCombo->itemData(index).toInt();
    m_model->setFilterConnectionId(id);
    onRowCountChanged();
}

void MessageLogView::onProtocolFilterChanged(int index)
{
    const int raw = m_protocolCombo->itemData(index).toInt();
    m_model->setFilterProtocol(static_cast<Message::Protocol>(raw));
    onRowCountChanged();
}

void MessageLogView::onDirectionFilterChanged(int index)
{
    const int direction = m_directionCombo->itemData(index).toInt();
    m_model->setFilterDirection(direction);
    onRowCountChanged();
}

void MessageLogView::onSearchTextChanged(const QString &text)
{
    m_model->setFilterText(text);
    onRowCountChanged();
}

void MessageLogView::onClearClicked()
{
    m_model->clear();
    onRowCountChanged();
}

void MessageLogView::onRowsInserted()
{
    onRowCountChanged();
    scrollToBottomIfNeeded();
}

void MessageLogView::onTableDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    // Запрашиваем оригинальное сообщение из модели через кастомную роль
    QVariant data = m_model->data(index, MessageLogModel::RawMessageRole);
    if (data.canConvert<Message>()) {
        emit messageDoubleClicked(data.value<Message>());
    }
}

void MessageLogView::onRowCountChanged()
{
    // Получаем количество строк после применения всех фильтров
    const int count = m_model->rowCount();
    m_countLabel->setText(
        QString("%1 message%2").arg(count).arg(count == 1 ? "" : "s")
        );
}

void MessageLogView::onTopicFilterChanged(const QString &text)
{
    m_model->setFilterTopic(text.trimmed());
    onRowCountChanged();
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void MessageLogView::scrollToBottomIfNeeded()
{
    if (!m_autoScrollCheck->isChecked()) {
        return;
    }

    // scrollToBottom() вызываем через QScrollBar чтобы не создавать
    // лишний QModelIndex для последней строки
    QScrollBar *vsb = m_tableView->verticalScrollBar();
    if (vsb) {
        vsb->setValue(vsb->maximum());
    }
}