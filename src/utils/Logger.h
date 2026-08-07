#ifndef NETSCOPE_LOGGER_H
#define NETSCOPE_LOGGER_H

#include <QObject>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QtGlobal>

// ---------------------------------------------------------------------------
// Logger — потокобезопасный файловый логгер
//
// Два режима использования:
//
//   1. Перехват стандартного Qt-вывода через installMessageHandler():
//      после вызова все qDebug() / qWarning() / qCritical() / qFatal()
//      пишутся в файл автоматически Подключается один раз в main()
//
//   2. Прямая запись через log():
//      Logger::instance().log(Logger::Info, "MyClass", "message");
//
// Паттерн: Singleton через статический метод instance()
// Потокобезопасность: QMutex защищает запись в файл
//
// todo: добавить новый уровень лога = одна строка в enum Level
// и одна ветка в levelToString() Формат строки меняется в formatEntry()
// ---------------------------------------------------------------------------
class Logger : public QObject
{
    Q_OBJECT

public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Critical,
        Fatal
    };
    Q_ENUM(Level)

    // Singleton — единственный экземпляр на процесс
    // Не копируется, не перемещается
    static Logger &instance();

    // Запрещаем копирование явно
    Logger(const Logger &)            = delete;
    Logger &operator=(const Logger &) = delete;

    // --- Настройка ---

    // Открыть файл для записи Если файл существует — дописываем в конец
    // Возвращает false если файл не удалось открыть
    bool openFile(const QString &filePath);

    // Закрыть файл явно (иначе закроется в деструкторе)
    void closeFile();

    bool isFileOpen() const;
    QString filePath() const;

    // Минимальный уровень для записи Сообщения ниже уровня игнорируются
    // По умолчанию Debug (писать всё)
    Level minimumLevel() const;
    void  setMinimumLevel(Level level);

    // Установить как обработчик Qt-сообщений
    // После вызова qDebug/qWarning/qCritical пишутся через Logger
    // Вызывать один раз в main() после открытия файла
    static void installMessageHandler();

    // Восстановить стандартный Qt-обработчик сообщений
    static void uninstallMessageHandler();

    // --- Запись ---

    // Основной метод записи Потокобезопасен
    void log(Level level, const QString &category, const QString &message);

    // Удобные обёртки
    void debug   (const QString &category, const QString &message);
    void info    (const QString &category, const QString &message);
    void warning (const QString &category, const QString &message);
    void critical(const QString &category, const QString &message);

signals:
    // Новая строка записана — для отображения лога в UI в будущем
    // Сигнал эмитируется из любого потока — подключать через Qt::QueuedConnection
    void entryLogged(Logger::Level level, const QString &entry);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;

    // Qt message handler — статическая функция требуемой сигнатуры
    static void qtMessageHandler(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &message);

    static QString levelToString(Level level);
    static QString formatEntry  (Level level,
                               const QString &category,
                               const QString &message);
    static Level   qtMsgTypeToLevel(QtMsgType type);

    mutable QMutex  m_mutex;       // mutable — нужен в const методах
    QFile           m_file;
    QTextStream     m_stream;
    Level           m_minimumLevel { Level::Debug };
    bool            m_fileOpen     { false };
};

#endif // NETSCOPE_LOGGER_H
