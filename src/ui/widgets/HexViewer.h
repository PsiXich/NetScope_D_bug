#ifndef NETSCOPE_HEXVIEWER_H
#define NETSCOPE_HEXVIEWER_H

#include "core/Message.h"

#include <QWidget>
#include <QByteArray>

class QTextEdit;
class QLabel;
class QSpinBox;
class QPushButton;
class QCheckBox;

// ---------------------------------------------------------------------------
// HexViewer — виджет для отображения бинарных и текстовых данных
//
// Два режима отображения (переключаются кнопками):
//
//   Hex + ASCII (классический hex dump):
//   ┌────────────────────────────────────────────────┐
//   │ Offset    00 01 02 03 04 05 06 07  ASCII       │
//   │ 00000000  48 65 6C 6C 6F 20 57 6F  Hello Wo    │
//   │ 00000008  72 6C 64 21 00 00 00 00  rld!....    │
//   └────────────────────────────────────────────────┘
//
//   Text (UTF-8):
//   ┌────────────────────────────────────────────────┐
//   │ Hello World!                                   │
//   └────────────────────────────────────────────────┘
//
// Используется как:
//   1. Встроенный виджет в нижней части панели (dock)
//   2. Отдельное диалоговое окно при двойном клике в MessageLogView
//
// Данные устанавливаются через setData() или setMessage()
// Виджет не хранит историю — только текущее сообщение
// ---------------------------------------------------------------------------
class HexViewer : public QWidget
{
    Q_OBJECT

public:
    enum class DisplayMode {
        HexDump,    // offset + hex + ascii
        Text        // plain UTF-8
    };

    explicit HexViewer(QWidget *parent = nullptr);

    // Установить данные для отображения
    void setData(const QByteArray &data, bool isText = false);

    // Удобная обёртка — берёт payload и isText из Message
    void setMessage(const Message &message);

    // Очистить виджет
    void clear();

    DisplayMode displayMode() const;
    void        setDisplayMode(DisplayMode mode);

    // Количество байт в строке hex dump (8 или 16)
    int  bytesPerRow() const;
    void setBytesPerRow(int count);

public slots:
    // Подключается к MessageLogView::messageDoubleClicked
    void onMessageDoubleClicked(const Message &message);

private slots:
    void onHexModeClicked();
    void onTextModeClicked();
    void onCopyClicked();
    void onBytesPerRowChanged(int value);

private:
    void setupUi();
    void setupConnections();
    void refresh();

    // Генерация hex dump строки
    QString buildHexDump(const QByteArray &data, int bytesPerRow) const;

    // Экранирование непечатаемых символов для ASCII-колонки
    static QChar toPrintable(char c);

    QByteArray   m_data;
    bool         m_isText       { false };
    DisplayMode  m_displayMode  { DisplayMode::HexDump };
    int          m_bytesPerRow  { 16 };

    // --- Toolbar ---
    QPushButton *m_hexModeBtn      { nullptr };
    QPushButton *m_textModeBtn     { nullptr };
    QSpinBox    *m_bytesPerRowSpin { nullptr };
    QPushButton *m_copyBtn         { nullptr };
    QLabel      *m_sizeLabel       { nullptr };

    // --- Область отображения ---
    QTextEdit   *m_textEdit        { nullptr };
};

#endif // NETSCOPE_HEXVIEWER_H