#ifndef MYSYSLOG_H
#define MYSYSLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Уровни логирования */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} LogLevel;

/* Инициализация системы логирования (можно указать имя приложения) */
void mysyslog_init(const char *identity);

/* Завершение работы системы логирования (освобождение ресурсов) */
void mysyslog_close(void);

/* Запись сообщения в лог
   level - уровень важности сообщения 
   fmt - форматная строка (как в printf)
   ... - аргументы для форматной строки */
void mysyslog(LogLevel level, const char *fmt, ...);

#endif /* MYSYSLOG_H */
