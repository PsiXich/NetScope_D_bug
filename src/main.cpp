#include "ui/MainWindow.h"
#include "core/Message.h"
#include "utils/Logger.h"
#include "core/connectionmanager.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

// ---------------------------------------------------------------------------
// setupLogger — настраивает файловый логгер до создания MainWindow
//
// Вынесено из main() чтобы не загромождать точку входа
// Возвращает путь к файлу лога для отображения в UI (в будущем)
// ---------------------------------------------------------------------------
static QString setupLogger()
{
    // Директория для логов: AppData/NetScopeDebug/logs/ (Win)
    //                       ~/.local/share/NetScopeDebug/logs/ (Linux)
    //                       ~/Library/Application Support/NetScopeDebug/logs/ (macOS)
    const QString logDir = QStandardPaths::writableLocation(
                               QStandardPaths::AppDataLocation)
                           + QStringLiteral("/logs");

    QDir().mkpath(logDir);  // создаём директорию если не существует

    // Имя файла содержит дату — один файл на день
    // При многократном запуске в один день сессии дописываются в конец
    const QString fileName = QString("netscope_%1.log")
                                 .arg(QDateTime::currentDateTime()
                                          .toString("yyyy-MM-dd"));

    const QString filePath = logDir + QChar('/') + fileName;

    Logger &logger = Logger::instance();
    logger.setMinimumLevel(Logger::Level::Debug);

    if (!logger.openFile(filePath)) {
        // Не критично — qDebug() продолжит работать через stderr
        qWarning() << "[main] Could not open log file:" << filePath;
    }

    // Перехватываем все qDebug/qWarning/qCritical после открытия файла
    Logger::installMessageHandler();

    return filePath;
}

// ---------------------------------------------------------------------------
// registerMetaTypes — регистрация пользовательских типов для Qt мета-системы
//
// Вызывается один раз до создания любых QObject
// Без регистрации типы нельзя передавать через Qt::QueuedConnection
// ---------------------------------------------------------------------------
static void registerMetaTypes()
{
    // Message и его вложенные enum
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<Message::Direction>("Message::Direction");
    qRegisterMetaType<Message::Protocol>("Message::Protocol");

    // ConnectionInfo для сигналов ConnectionManager
    qRegisterMetaType<ConnectionInfo>("ConnectionInfo");

    // Logger::Level для сигнала entryLogged()
    qRegisterMetaType<Logger::Level>("Logger::Level");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // QApplication должен быть создан до любых виджетов и до обращения
    // к QStandardPaths на некоторых платформах.
    QApplication app(argc, argv);

    // Метаданные приложения — используются QStandardPaths для формирования
    // путей к AppData и другим директориям
    QApplication::setOrganizationName("NetScopeDebug");
    QApplication::setOrganizationDomain("netscope.local");
    QApplication::setApplicationName("NetScopeDebug");
    QApplication::setApplicationVersion(NETSCOPE_VERSION_STR);

    // Порядок вызовов важен:
    // 1. registerMetaTypes — до любых соединений сигнал/слот
    // 2. setupLogger       — до создания окна (окно может логировать в конструкторе)
    // 3. MainWindow        — после всей инфраструктуры
    registerMetaTypes();

    const QString logPath = setupLogger();
    Logger::instance().info("main",
                            QString("NetScopeDebug %1 starting. Log: %2")
                                .arg(NETSCOPE_VERSION_STR)
                                .arg(logPath)
                            );

    MainWindow window;
    window.show();

    const int exitCode = app.exec();

    Logger::instance().info("main",
                            QString("Application exiting with code %1").arg(exitCode)
                            );

    // Восстанавливаем стандартный Qt-обработчик до удаления логгера.
    // После uninstall qDebug() в деструкторах снова идёт в stderr.
    Logger::uninstallMessageHandler();

    return exitCode;
}