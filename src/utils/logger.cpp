#include "Logger.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QTextStream>
#include <iostream>

// ---------------------------------------------------------------------------
// Logger implementation
// ---------------------------------------------------------------------------

Logger &Logger::instance()
{
    // Инициализация при первом вызове — потокобезопасно в C++11 и выше
    // static local variable initialization гарантированно атомарна
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
    // Подключаем поток к файлу — stream будет настроен в openFile()
    m_stream.setDevice(&m_file);

    // UTF-8 кодировка: API различается между Qt5 и Qt6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_stream.setEncoding(QStringConverter::Utf8);
#else
    m_stream.setCodec("UTF-8");
#endif
}

Logger::~Logger()
{
    closeFile();
}

// ---------------------------------------------------------------------------
// Настройка
// ---------------------------------------------------------------------------

bool Logger::openFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);

    if (m_fileOpen) {
        // Уже открыт другой файл — закрываем его сначала
        m_stream.flush();
        m_file.close();
        m_fileOpen = false;
    }

    m_file.setFileName(filePath);

    // QIODevice::Append — дописываем в конец Не затираем лог при перезапуске
    // QIODevice::Text   — нормализует переводы строк под платформу
    if (!m_file.open(QIODevice::Append | QIODevice::Text)) {
        std::cerr << "[Logger] Failed to open log file: "
                  << filePath.toStdString() << std::endl;
        return false;
    }

    m_fileOpen = true;

    // Разделитель сессий — удобно видеть где начался новый запуск
    m_stream << "\n"
             << "======================================\n"
             << "  Session started: "
             << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << "\n"
             << "======================================\n";
    m_stream.flush();

    return true;
}

void Logger::closeFile()
{
    QMutexLocker locker(&m_mutex);

    if (!m_fileOpen) {
        return;
    }

    m_stream << "======================================\n"
             << "  Session ended:   "
             << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << "\n"
             << "======================================\n\n";
    m_stream.flush();
    m_file.close();
    m_fileOpen = false;
}

bool Logger::isFileOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_fileOpen;
}

QString Logger::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.fileName();
}

Logger::Level Logger::minimumLevel() const
{
    QMutexLocker locker(&m_mutex);
    return m_minimumLevel;
}

void Logger::setMinimumLevel(Level level)
{
    QMutexLocker locker(&m_mutex);
    m_minimumLevel = level;
}

// ---------------------------------------------------------------------------
// Установка Qt message handler
// ---------------------------------------------------------------------------

void Logger::installMessageHandler()
{
    qInstallMessageHandler(Logger::qtMessageHandler);
}

void Logger::uninstallMessageHandler()
{
    // nullptr восстанавливает стандартный Qt-обработчик
    qInstallMessageHandler(nullptr);
}

// ---------------------------------------------------------------------------
// Запись
// ---------------------------------------------------------------------------

void Logger::log(Level level, const QString &category, const QString &message)
{
    QMutexLocker locker(&m_mutex);

    if (level < m_minimumLevel) {
        return;
    }

    const QString entry = formatEntry(level, category, message);

    // Пишем в файл если открыт
    if (m_fileOpen) {
        m_stream << entry << "\n";
        m_stream.flush();   // flush на каждой строке — важно чтобы не потерять
        // записи при краше При высокой нагрузке
        // делать flush по таймеру
    }

    // Дублируем в stderr — удобно при разработке без открытого файла
    std::cerr << entry.toStdString() << std::endl;

    // Эмитируем сигнал без мьютекса — он уже захвачен выше
    // Используем QMetaObject::invokeMethod чтобы выйти из-под мьютекса
    // перед доставкой сигнала и избежать дедлока если слот тоже пишет в лог
    // Разблокируем мьютекс перед emit
    locker.unlock();
    emit entryLogged(level, entry);
}

void Logger::debug(const QString &category, const QString &message)
{
    log(Level::Debug, category, message);
}

void Logger::info(const QString &category, const QString &message)
{
    log(Level::Info, category, message);
}

void Logger::warning(const QString &category, const QString &message)
{
    log(Level::Warning, category, message);
}

void Logger::critical(const QString &category, const QString &message)
{
    log(Level::Critical, category, message);
}

// ---------------------------------------------------------------------------
// Приватные статические методы
// ---------------------------------------------------------------------------

void Logger::qtMessageHandler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &message)
{
    // context.category доступна через Q_LOGGING_CATEGORY
    // Если категория не задана — используем имя файла как fallback
    QString category;
    if (context.category && qstrlen(context.category) > 0) {
        category = QString::fromLatin1(context.category);
    } else if (context.file && qstrlen(context.file) > 0) {
        // Берём только имя файла без пути
        const QString fileFull = QString::fromLatin1(context.file);
        const int lastSlash = qMax(
            fileFull.lastIndexOf('/'),
            fileFull.lastIndexOf('\\')
            );
        category = fileFull.mid(lastSlash + 1);
    } else {
        category = QStringLiteral("qt");
    }

    const Level level = qtMsgTypeToLevel(type);
    Logger::instance().log(level, category, message);

    // qFatal() должен завершить процесс — восстанавливаем стандартное поведение
    if (type == QtFatalMsg) {
        std::abort();
    }
}

QString Logger::levelToString(Level level)
{
    switch (level) {
    case Level::Debug:    return QStringLiteral("DBG");
    case Level::Info:     return QStringLiteral("INF");
    case Level::Warning:  return QStringLiteral("WRN");
    case Level::Critical: return QStringLiteral("CRT");
    case Level::Fatal:    return QStringLiteral("FTL");
    }
    return QStringLiteral("???");
}

QString Logger::formatEntry(Level level,
                            const QString &category,
                            const QString &message)
{
    // Формат: "2024-01-15 14:23:45.123 [WRN] TcpClient: connection refused"
    return QString("%1 [%2] %3: %4")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(levelToString(level))
        .arg(category)
        .arg(message);
}

Logger::Level Logger::qtMsgTypeToLevel(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return Level::Debug;
    case QtInfoMsg:     return Level::Info;
    case QtWarningMsg:  return Level::Warning;
    case QtCriticalMsg: return Level::Critical;
    case QtFatalMsg:    return Level::Fatal;
    default:            return Level::Debug;
    }
}