#include "HexViewer.h"

#include <QTextEdit>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QDebug>

// ---------------------------------------------------------------------------
// HexViewer implementation
// ---------------------------------------------------------------------------

HexViewer::HexViewer(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

// ---------------------------------------------------------------------------
// Публичные методы
// ---------------------------------------------------------------------------

void HexViewer::setData(const QByteArray &data, bool isText)
{
    m_data   = data;
    m_isText = isText;

    // Если данные текстовые — переключаемся в Text режим автоматически
    // Пользователь может явно вернуться в HexDump если нужно
    if (isText && m_displayMode != DisplayMode::Text) {
        m_displayMode = DisplayMode::Text;
        m_textModeBtn->setChecked(true);
        m_hexModeBtn->setChecked(false);
        // bytesPerRow не актуален в Text режиме
        m_bytesPerRowSpin->setEnabled(false);
    } else if (!isText && m_displayMode != DisplayMode::HexDump) {
        m_displayMode = DisplayMode::HexDump;
        m_hexModeBtn->setChecked(true);
        m_textModeBtn->setChecked(false);
        m_bytesPerRowSpin->setEnabled(true);
    }

    refresh();
}

void HexViewer::setMessage(const Message &message)
{
    // Системные сообщения — показываем info строку как текст
    if (message.direction == Message::Direction::System) {
        setData(message.info.toUtf8(), true);
        return;
    }

    setData(message.payload, message.isText);
}

void HexViewer::clear()
{
    m_data.clear();
    m_isText = false;
    m_textEdit->clear();
    m_sizeLabel->setText("0 bytes");
}

HexViewer::DisplayMode HexViewer::displayMode() const
{
    return m_displayMode;
}

void HexViewer::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode == mode) {
        return;
    }
    m_displayMode = mode;

    const bool isHex = (mode == DisplayMode::HexDump);
    m_hexModeBtn->setChecked(isHex);
    m_textModeBtn->setChecked(!isHex);
    m_bytesPerRowSpin->setEnabled(isHex);

    refresh();
}

int HexViewer::bytesPerRow() const
{
    return m_bytesPerRow;
}

void HexViewer::setBytesPerRow(int count)
{
    if (count < 1) {
        count = 1;
    }
    m_bytesPerRow = count;
    m_bytesPerRowSpin->setValue(count);
    refresh();
}

// ---------------------------------------------------------------------------
// Публичные слоты
// ---------------------------------------------------------------------------

void HexViewer::onMessageDoubleClicked(const Message &message)
{
    setMessage(message);
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void HexViewer::setupUi()
{
    // -----------------------------------------------------------------------
    // Toolbar
    // -----------------------------------------------------------------------
    m_hexModeBtn = new QPushButton("Hex", this);
    m_hexModeBtn->setCheckable(true);
    m_hexModeBtn->setChecked(true);
    m_hexModeBtn->setFixedWidth(50);

    m_textModeBtn = new QPushButton("Text", this);
    m_textModeBtn->setCheckable(true);
    m_textModeBtn->setChecked(false);
    m_textModeBtn->setFixedWidth(50);

    m_bytesPerRowSpin = new QSpinBox(this);
    m_bytesPerRowSpin->setRange(4, 32);
    m_bytesPerRowSpin->setValue(m_bytesPerRow);
    m_bytesPerRowSpin->setSingleStep(4);    // шаг кратный 4 — стандарт для hex dump
    m_bytesPerRowSpin->setPrefix("Row: ");
    m_bytesPerRowSpin->setFixedWidth(90);

    m_copyBtn = new QPushButton("Copy", this);
    m_copyBtn->setFixedWidth(55);
    m_copyBtn->setToolTip("Copy content to clipboard");

    m_sizeLabel = new QLabel("0 bytes", this);
    m_sizeLabel->setStyleSheet("color: #666;");
    m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout *toolRow = new QHBoxLayout;
    toolRow->addWidget(m_hexModeBtn);
    toolRow->addWidget(m_textModeBtn);
    toolRow->addSpacing(8);
    toolRow->addWidget(new QLabel("Bytes/row:", this));
    toolRow->addWidget(m_bytesPerRowSpin);
    toolRow->addSpacing(8);
    toolRow->addWidget(m_copyBtn);
    toolRow->addStretch();
    toolRow->addWidget(m_sizeLabel);

    // -----------------------------------------------------------------------
    // Область отображения
    // -----------------------------------------------------------------------
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QTextEdit::NoWrap);     // горизонтальный скролл для hex dump

    // Моноширинный шрифт — обязателен для выравнивания колонок hex dump
    QFont monoFont("Courier New", 10);
    monoFont.setStyleHint(QFont::Monospace);            // fallback если Courier New недоступен
    m_textEdit->setFont(monoFont);

    // -----------------------------------------------------------------------
    // Корневой layout
    // -----------------------------------------------------------------------
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 4, 0, 0);
    root->addLayout(toolRow);
    root->addWidget(m_textEdit, 1);
}

void HexViewer::setupConnections()
{
    connect(m_hexModeBtn, SIGNAL(clicked()),
            this, SLOT(onHexModeClicked()));

    connect(m_textModeBtn, SIGNAL(clicked()),
            this, SLOT(onTextModeClicked()));

    connect(m_copyBtn, SIGNAL(clicked()),
            this, SLOT(onCopyClicked()));

    connect(m_bytesPerRowSpin, SIGNAL(valueChanged(int)),
            this, SLOT(onBytesPerRowChanged(int)));
}

// ---------------------------------------------------------------------------
// Приватные слоты
// ---------------------------------------------------------------------------

void HexViewer::onHexModeClicked()
{
    setDisplayMode(DisplayMode::HexDump);
}

void HexViewer::onTextModeClicked()
{
    setDisplayMode(DisplayMode::Text);
}

void HexViewer::onCopyClicked()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_textEdit->toPlainText());
    }
}

void HexViewer::onBytesPerRowChanged(int value)
{
    m_bytesPerRow = value;
    if (m_displayMode == DisplayMode::HexDump) {
        refresh();
    }
}

// ---------------------------------------------------------------------------
// Приватные методы
// ---------------------------------------------------------------------------

void HexViewer::refresh()
{
    if (m_data.isEmpty()) {
        m_textEdit->setPlainText("<no data>");
        m_sizeLabel->setText("0 bytes");
        return;
    }

    const int size = m_data.size();
    m_sizeLabel->setText(
        QString("%1 byte%2").arg(size).arg(size == 1 ? "" : "s")
        );

    if (m_displayMode == DisplayMode::Text) {
        // Пытаемся декодировать как UTF-8
        // fromUtf8() заменяет невалидные байты на U+FFFD — не крашится
        m_textEdit->setPlainText(QString::fromUtf8(m_data));
    } else {
        m_textEdit->setPlainText(buildHexDump(m_data, m_bytesPerRow));
    }
}

QString HexViewer::buildHexDump(const QByteArray &data, int bytesPerRow) const
{
    if (data.isEmpty()) {
        return QString();
    }

    // Заголовок: "Offset   00 01 02 ... 0F   ASCII"
    // Ширина hex-части: bytesPerRow * 3 символа (2 hex + 1 пробел)
    QString header = "Offset    ";
    for (int i = 0; i < bytesPerRow; ++i) {
        header += QString("%1 ").arg(i, 2, 16, QChar('0')).toUpper();
    }
    header += "  ASCII\n";
    header += QString(header.length() - 1, '-') + "\n";

    QString result;
    // Резервируем примерный объём чтобы не реаллоцировать
    result.reserve(data.size() * 4 + header.length());
    result += header;

    const int totalRows = (data.size() + bytesPerRow - 1) / bytesPerRow;

    for (int row = 0; row < totalRows; ++row) {
        const int offset    = row * bytesPerRow;
        const int rowEnd    = qMin(offset + bytesPerRow, data.size());
        const int rowLength = rowEnd - offset;

        // Offset колонка: "00000000  "
        result += QString("%1  ")
                      .arg(offset, 8, 16, QChar('0'))
                      .toUpper();

        // Hex колонка
        for (int i = 0; i < bytesPerRow; ++i) {
            if (i < rowLength) {
                result += QString("%1 ")
                .arg(static_cast<quint8>(data.at(offset + i)),
                     2, 16, QChar('0'))
                    .toUpper();
            } else {
                // Дополняем пробелами последнюю неполную строку
                result += "   ";
            }
        }

        result += "  ";

        // ASCII колонка
        for (int i = 0; i < rowLength; ++i) {
            result += toPrintable(data.at(offset + i));
        }

        result += "\n";
    }

    return result;
}

// static
QChar HexViewer::toPrintable(char c)
{
    const uchar uc = static_cast<uchar>(c);
    // Печатаемые ASCII: 0x20 (пробел) до 0x7E (~)
    // Всё остальное заменяем точкой — стандарт hex редакторов
    return (uc >= 0x20 && uc <= 0x7E) ? QChar(c) : QChar('.');
}