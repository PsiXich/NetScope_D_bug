#include "Theme.h"
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

void Theme::apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#211f2e"));
    palette.setColor(QPalette::WindowText, QColor("#eeeaf7"));
    palette.setColor(QPalette::Base, QColor("#1c1a27"));
    palette.setColor(QPalette::AlternateBase, QColor("#282535"));
    palette.setColor(QPalette::Text, QColor("#eeeaf7"));
    palette.setColor(QPalette::Button, QColor("#302d40"));
    palette.setColor(QPalette::ButtonText, QColor("#eeeaf7"));
    palette.setColor(QPalette::Highlight, QColor("#514966"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#b6a6e8"));
    palette.setColor(QPalette::ToolTipBase, QColor("#302d40"));
    palette.setColor(QPalette::ToolTipText, QColor("#eeeaf7"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a19bb4"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#797387"));
    app.setPalette(palette);
    app.setFont(QFont("Segoe UI", 10));
    app.setStyleSheet(QStringLiteral(R"(
        QToolTip { border: 1px solid #625972; padding: 6px; }
        QToolBar { background: #272434; border: none; spacing: 5px; padding: 8px; }
        QToolButton { border: 1px solid transparent; border-radius: 6px; padding: 7px 10px; color: #c2b6e4; }
        QToolButton:hover { background: #383347; }
        QToolButton:pressed, QToolButton:checked { background: #514966; }
        QPushButton { background: #353044; border: 1px solid #494157; border-radius: 6px; padding: 6px 12px; min-height: 18px; }
        QPushButton:hover { background: #443c57; border-color: #a99bd0; }
        QPushButton:pressed { background: #514966; }
        QPushButton:focus { border-color: #b6a6e8; }
        QPushButton:disabled { background: #292634; border-color: #373241; }
        QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QComboBox {
            background: #1c1a27; border: 1px solid #433d53; border-radius: 5px; padding: 5px; selection-background-color: #514966;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #b6a6e8; }
        QLineEdit:disabled, QSpinBox:disabled { background: #252230; }
        QComboBox QAbstractItemView { background: #302d40; selection-background-color: #514966; border: 1px solid #625972; }
        QGroupBox { border: 1px solid #433d53; border-radius: 7px; margin-top: 13px; padding: 12px 8px 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: #c2b6e4; }
        QTabWidget::pane { border: 1px solid #433d53; border-radius: 6px; top: -1px; }
        QTabBar::tab { background: #2b2839; color: #b2a8ca; border: 1px solid #433d53; border-top-left-radius: 6px; border-top-right-radius: 6px; padding: 7px 16px; margin-right: 3px; }
        QTabBar::tab:selected { background: #494453; color: #f4f0fc; }
        QTabBar::tab:hover:!selected { background: #383347; }
        QAbstractItemView { border: 1px solid #433d53; border-radius: 5px; outline: none; selection-background-color: #514966; selection-color: #ffffff; }
        QListView::item { padding: 8px 6px; border-radius: 4px; }
        QListView::item:hover { background: #353044; }
        QHeaderView::section { background: #302c3e; color: #bcb1d5; border: none; border-right: 1px solid #433d53; padding: 6px; }
        QTableCornerButton::section { background: #302c3e; border: none; }
        QSplitter::handle { background: #302b3e; }
        QSplitter::handle:hover { background: #625477; }
        QStatusBar { background: #272434; color: #b2a8ca; }
        QStatusBar::item { border: none; }
        QScrollBar:vertical { background: #211f2e; width: 12px; margin: 0; }
        QScrollBar:horizontal { background: #211f2e; height: 12px; margin: 0; }
        QScrollBar::handle { background: #50485f; border-radius: 5px; min-width: 24px; min-height: 24px; }
        QScrollBar::handle:hover { background: #73658d; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QMenu { background: #2b2839; border: 1px solid #494157; padding: 4px; }
        QMenu::item { padding: 6px 20px; }
        QMenu::item:selected { background: #514966; }
    )"));
}
