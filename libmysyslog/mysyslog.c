#define _XOPEN_SOURCE 700
#include "mysyslog.h"
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

/* Глобальные (статические) переменные для настроек логирования */
static const char *log_identity = NULL;
static int log_to_syslog = 0;  /* Флаг использования системного syslog (0 - вывод в stderr) */

#ifdef linux
#include <syslog.h>
#endif

void mysyslog_init(const char *identity) {
    log_identity = identity;
    // Если нужно, можно добавить определение, использовать ли системный syslog.
    // Пока что будем выводить сообщения на stderr.
    log_to_syslog = 0;
#ifdef linux
    // Можно раскомментировать следующую строку, чтобы использовать системный журнал:
    // openlog(log_identity, LOG_PID | LOG_CONS, LOG_USER);
#endif
}

void mysyslog_close(void) {
#ifdef linux
    // Если использовали системный syslog, закрываем.
    if (log_to_syslog) {
        closelog();
    }
#endif
    log_identity = NULL;
}

/* Приватная функция для получения текущего времени в виде строки */
static void get_current_time(char *buffer, size_t buflen) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    snprintf(buffer, buflen, "%02d.%02d.%04d %02d:%02d:%02d",
             tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void mysyslog(LogLevel level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Формируем префикс сообщения по уровню
    const char *level_str = "";
    switch (level) {
        case LOG_LEVEL_DEBUG:   level_str = "ОТЛАДКА"; break;
        case LOG_LEVEL_INFO:    level_str = "ИНФО"; break;
        case LOG_LEVEL_WARNING: level_str = "ПРЕДУПРЕЖДЕНИЕ"; break;
        case LOG_LEVEL_ERROR:   level_str = "ОШИБКА"; break;
    }

    char time_str[64];
    get_current_time(time_str, sizeof(time_str));

    if (log_to_syslog) {
#ifdef linux
        int priority;
        switch (level) {
            case LOG_LEVEL_DEBUG:   priority = LOG_DEBUG; break;
            case LOG_LEVEL_INFO:    priority = LOG_INFO; break;
            case LOG_LEVEL_WARNING: priority = LOG_WARNING; break;
            case LOG_LEVEL_ERROR:   priority = LOG_ERR; break;
            default:                priority = LOG_INFO; break;
        }
        char msgbuf[1024];
        vsnprintf(msgbuf, sizeof(msgbuf), fmt, args);
        syslog(priority, "%s", msgbuf);
#endif
    } else {
        // Выводим лог на stderr
        fprintf(stderr, "%s [%s] ", time_str, level_str);
        if (log_identity) {
            fprintf(stderr, "%s: ", log_identity);
        }
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    va_end(args);
}
