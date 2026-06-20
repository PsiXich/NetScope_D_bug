#ifndef NETSCOPE_CONNECTIONSTATS_H
#define NETSCOPE_CONNECTIONSTATS_H

#include <QDateTime>
#include <QString>

// ---------------------------------------------------------------------------
// ConnectionStats — статистика одного соединения
//
// Plain struct — не QObject, хранится в QMap внутри ConnectionManager
// Обновляется при каждом входящем/исходящем сообщении
// ---------------------------------------------------------------------------
struct ConnectionStats
{
    // -----------------------------------------------------------------------
    // Счётчики трафика
    // -----------------------------------------------------------------------
    quint64 bytesIn          { 0 };  // суммарно принято байт
    quint64 bytesOut         { 0 };  // суммарно отправлено байт
    quint64 messagesIn       { 0 };  // количество входящих сообщений
    quint64 messagesOut      { 0 };  // количество исходящих сообщений

    // -----------------------------------------------------------------------
    // Временные метки
    // -----------------------------------------------------------------------

    // Момент первого подключения за сессию
    // Не сбрасывается при реконнекте — отражает время создания соединения
    QDateTime connectedAt;

    // Последняя активность (любое входящее или исходящее сообщение)
    QDateTime lastActivityAt;

    // -----------------------------------------------------------------------
    // Пиковая скорость (байт/сек)
    // Обновляется в ConnectionManager при каждом сообщении
    // Используется как reference точка для графиков в будущем
    // -----------------------------------------------------------------------
    quint64 peakBytesPerSecIn  { 0 };
    quint64 peakBytesPerSecOut { 0 };

    // -----------------------------------------------------------------------
    // Вспомогательные методы — форматирование для UI
    // -----------------------------------------------------------------------

    // Суммарный трафик в обе стороны
    quint64 totalBytes() const
    {
        return bytesIn + bytesOut;
    }

    quint64 totalMessages() const
    {
        return messagesIn + messagesOut;
    }

    // Форматирование байт в читаемый вид: B / KB / MB
    static QString formatBytes(quint64 bytes)
    {
        if (bytes < 1024) {
            return QString("%1 B").arg(bytes);
        }
        if (bytes < 1024 * 1024) {
            return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        }
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    }

    // Время с момента последней активности
    QString idleTimeString() const
    {
        if (!lastActivityAt.isValid()) {
            return QStringLiteral("—");
        }

        const qint64 secs = lastActivityAt.secsTo(
            QDateTime::currentDateTimeUtc()
            );

        if (secs < 60)  return QString("%1s ago").arg(secs);
        if (secs < 3600) return QString("%1m ago").arg(secs / 60);
        return QString("%1h ago").arg(secs / 3600);
    }

    // Сброс всех счётчиков (например по кнопке в StatsWidget)
    void reset()
    {
        bytesIn           = 0;
        bytesOut          = 0;
        messagesIn        = 0;
        messagesOut       = 0;
        peakBytesPerSecIn = 0;
        peakBytesPerSecOut = 0;
        lastActivityAt    = QDateTime();
        // connectedAt не сбрасываем — это время жизни соединения
    }
};

#endif // NETSCOPE_CONNECTIONSTATS_H