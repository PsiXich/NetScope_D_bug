#include "Message.h"

// ---------------------------------------------------------------------------
// Файл намеренно минимален
//
// Q_DECLARE_METATYPE объявляет тип, но не регистрирует его в рантайме
// Регистрация через qRegisterMetaType нужна для:
//   - передачи через Qt::QueuedConnection (межпоточные сигналы)
//   - использования в QVariant без явного приведения типов
//
// Вызов происходит в main() один раз до создания QApplication
// ---------------------------------------------------------------------------

// Функция вынесена сюда чтобы не засорять main.cpp деталями регистрации
// При добавлении новых типов — qRegisterMetaType сюда
void registerMessageMetaTypes()
{
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<Message::Direction>("Message::Direction");
    qRegisterMetaType<Message::Protocol>("Message::Protocol");
}