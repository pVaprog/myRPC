# Проект myRPC

myRPC — это система удалённого вызова команд (Remote Procedure Call) с использованием сокетов и протокола JSON. Реализована на языке C и предназначена для работы в Linux (включая Astra Linux, Ubuntu, Debian, Kali, Mint и др.).

Система состоит из:
- myRPC-server — демон, обрабатывающий запросы от клиентов;
- myRPC-client — консольная утилита для отправки команд;
- libmysyslog — библиотека логирования, используемая клиентом и сервером.

##Структура проекта
```
myRPC/
├── client/ 			# Исходники и сборка клиентского приложения
│ ├── myRPC-client.c 	# Исходный код клиента
│ ├── Makefile 			# Makefile для сборки клиента
│ └── README.md 		# Инструкция по использованию клиента
├── server/ 			# Исходники и сборка серверного приложения
│ ├── myRPC-server.c 	# Исходный код сервера
│ ├── Makefile 			# Makefile для сборки сервера
│ └── README.md 		# Инструкция по настройке и запуску сервера
├── libmysyslog/ 		# Вспомогательная библиотека логирования (аналог syslog)
│ ├── mysyslog.h 		# Заголовочный файл библиотеки
│ ├── mysyslog.c 		# Реализация функций логирования
│ ├── Makefile 			# Makefile для сборки библиотеки
│ └── README.md 		# Описание использования библиотеки
├── myRPC.conf 			# Конфигурационный файл сервера (пример)
├── users.conf 			# Файл со списком разрешённых пользователей (пример)
├── Makefile 			# Общий Makefile для сборки всего проекта и деб-пакета
└── README.md 			# Общий README с описанием проекта
```

##Сборка проекта

Для сборки **всех компонентов** выполните в корне проекта:

```bash
make
```

Или вручную по частям:
```
make -C libmysyslog
make -C client
make -C server
```

###Создание DEB-пакетов
Проект поддерживает сборку .deb-пакетов для установки через пакетный менеджер:
```
make deb
```

Появятся файлы:

*myrpc-client_<версия>.deb*
*myrpc-server_<версия>.deb*


###Установка
```
sudo dpkg -i myrpc-client_*.deb
sudo dpkg -i myrpc-server_*.deb
```

После установки:
myRPC-client и myRPC-server будут доступны в /usr/bin

Логи пишутся в /var/log/myrpc-client.log и /var/log/myrpc-server.log


###Конфигурация
Создайте папку и конфигурационные файлы:
```
sudo mkdir -p /etc/myRPC
```

/etc/myRPC/myRPC.conf

# Порт для соединения
port = 1234

# Тип сокета: stream или dgram
socket_type = stream

/etc/myRPC/users.conf

Список разрешённых пользователей (по одному в строке):

astraadmin

###Пример использования
Запуск сервера:
```
sudo myRPC-server
```
Запуск клиента:
```
myRPC-client --host 127.0.0.1 --port 1234 --stream --command "ls -l /tmp"
```
Также можно использовать UDP:
```
myRPC-client --host 127.0.0.1 --port 1234 --dgram --command "whoami"
```

###Протокол (JSON)
Запрос клиента:
```
{
  "login": "имя_пользователя",
  "command": "bash-команда"
}
```
Ответ сервера:
```
{
  "code": 0,            // 0 — успех, 1 — ошибка
  "result": "Результат"
}
```
###Логирование
Все действия записываются в лог-файлы через libmysyslog:

/var/log/myrpc-client.log

/var/log/myrpc-server.log


##Требования

libjson-c (установить: sudo apt install libjson-c-dev)

Права на запуск и чтение конфигов (sudo)

GCC, make, dpkg-dev


##Безопасность

Проверка пользователя через файл users.conf

Выполнение команд через /bin/sh -c

Вывод stdout и stderr сохраняется во временные файлы (/tmp/myRPC_XXXXXX.*)


##Аргументы клиента

Аргумент + Назначение

**--host** IP-адрес сервера


**--port** Порт сервера


**--command** Команда bash для выполнения


**--stream** Использовать TCP


**--dgram** Использовать UDP


**--help** Показать справку



##Очистка
```
make clean
```
Удалит все .o, бинарники, временные и сборочные файлы.

##Дополнительные модули
Каждый компонент проекта сопровождается своим README.md:

client/README.md
server/README.md
libmysyslog/README.md


###Авторы

Разработка: Студент РТУ МИРЭА Петрова Вера
GitHub: github.com/pVaprog
