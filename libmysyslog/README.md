# Библиотека libmysyslog

libmysyslog — это простая библиотека для логирования сообщений в файл журнала. Используется как клиентом myRPC-client, так и сервером myRPC-server для записи информационных и ошибочных сообщений.

##Назначение

Логирует:
- информационные события ([ИНФО])
- сообщения об ошибках ([ОШИБКА])

Вывод осуществляется в файл /var/log/<идентификатор>.log, где <идентификатор> задаётся при инициализации.

##Состав

- mysyslog.c — реализация функций логирования
- mysyslog.h — заголовочный файл с интерфейсом
- Makefile — сборка библиотеки

##Функции
```
void mysyslog_open(const char *ident);
void mysyslog_log_info(const char *format, ...);
void mysyslog_log_error(const char *format, ...);
void mysyslog_close();
```

**Использование**
Подключите заголовок:

#include "mysyslog.h"

Откройте лог-файл:

*mysyslog_open("myrpc-client");*


Запишите лог:

*mysyslog_log_info("Клиент запущен");*
*mysyslog_log_error("Ошибка подключения к серверу");*


Закройте лог:

*mysyslog_close();*

**Сборка**
```
make
```

Скомпилируется объектный файл libmysyslog.o, который подключается к клиенту и серверу.

**Примечания**

Библиотека не использует syslog — только собственный лог-файл.

Для работы требуется доступ на запись в /var/log.

Использует stdio.h, stdarg.h, time.h и стандартные функции C.
