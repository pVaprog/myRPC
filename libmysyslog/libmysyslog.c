/**
 * @file mysyslog.c
 * Реализация функций библиотеки libmysyslog для записи сообщений в syslog.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include "mysyslog.h"

static int log_opened = 0;

void my_openlog(const char *ident) {
    // Открываем соединение с syslog с указанным идентификатором программы
    openlog(ident, LOG_PID | LOG_CONS, LOG_USER);
    log_opened = 1;
}

void my_syslog(int priority, const char *format, ...) {
    if (!log_opened) {
        // Если openlog еще не вызывался, открываем с идентификатором по умолчанию
        my_openlog("myRPC");
    }
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

void my_closelog() {
    if (log_opened) {
        closelog();
        log_opened = 0;
    }
}
