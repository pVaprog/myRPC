#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <json-c/json.h>
#include "../libmysyslog/mysyslog.h"

/* Параметры, задаваемые через конфигурацию или аргументы */
static int server_port = 12345;
static int use_tcp = 1;  // 1 = TCP (SOCK_STREAM), 0 = UDP (SOCK_DGRAM)

/* Массив разрешённых пользователей */
static char **allowed_users = NULL;
static size_t allowed_count = 0;
static size_t allowed_capacity = 0;

/* Свободить список разрешённых пользователей */
static void free_allowed_users() {
    if (allowed_users) {
        for (size_t i = 0; i < allowed_count; ++i) {
            free(allowed_users[i]);
        }
        free(allowed_users);
        allowed_users = NULL;
        allowed_count = 0;
        allowed_capacity = 0;
    }
}

/* Добавить имя пользователя в список разрешённых */
static void add_allowed_user(const char *username) {
    if (!username || username[0] == '\0') return;
    // Расширить массив при необходимости
    if (allowed_count == allowed_capacity) {
        size_t new_cap = (allowed_capacity == 0 ? 10 : allowed_capacity * 2);
        char **new_list = realloc(allowed_users, new_cap * sizeof(char*));
        if (!new_list) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка: недостаточно памяти для списка пользователей");
            return;
        }
        allowed_users = new_list;
        allowed_capacity = new_cap;
    }
    // Копируем имя
    allowed_users[allowed_count] = strdup(username);
    if (allowed_users[allowed_count] == NULL) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка: недостаточно памяти для имени пользователя '%s'", username);
        return;
    }
    allowed_count++;
}

/* Загрузить файл разрешённых пользователей (users.conf) */
static int load_allowed_users(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Убираем конец строки
        size_t len = strlen(line);
        if (len == 0) continue;
        if (line[len-1] == '\n' || line[len-1] == '\r') {
            line[len-1] = '\0';
            len--;
        }
        // Пропустить пустые строки
        char *ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') continue;
        // Пропустить комментарии
        if (*ptr == '#') continue;
        // Обрезаем пробелы в конце
        char *end = ptr + strlen(ptr) - 1;
        while(end > ptr && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
        // Добавить пользователя
        add_allowed_user(ptr);
    }
    fclose(f);
    return 0;
}

/* Загрузить конфигурационный файл myRPC.conf */
static void load_config(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        mysyslog(LOG_LEVEL_WARNING, "Не удалось открыть файл конфигурации '%s': %s. Будут использованы настройки по умолчанию.", filename, strerror(errno));
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Удаляем перевод строки
        size_t len = strlen(line);
        if (len == 0) continue;
        if (line[len-1] == '\n' || line[len-1] == '\r') {
            line[len-1] = '\0';
        }
        // Пропускаем начальные пробелы
        char *ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') continue;
        if (*ptr == '#') continue; // комментарий
        // Ищем разделитель '='
        char *eq = strchr(ptr, '=');
        if (!eq) {
            mysyslog(LOG_LEVEL_WARNING, "Некорректная строка в конфигурации: \"%s\"", ptr);
            continue;
        }
        *eq = '\0';
        char *key = ptr;
        char *value = eq + 1;
        // Удаляем пробелы вокруг ключа
        while (isspace((unsigned char)*key)) key++;
        char *kend = key + strlen(key) - 1;
        while (kend > key && isspace((unsigned char)*kend)) {
            *kend = '\0';
            kend--;
        }
        // Удаляем пробелы вокруг значения
        while (isspace((unsigned char)*value)) value++;
        char *vend = value + strlen(value) - 1;
        while (vend > value && isspace((unsigned char)*vend)) {
            *vend = '\0';
            vend--;
        }
        // Преобразуем ключ в нижний регистр для сравнения
        for (char *p = key; *p; ++p) *p = tolower((unsigned char)*p);
        // Обработка известных опций
        if (strcmp(key, "port") == 0) {
            int port_val = atoi(value);
            if (port_val > 0 && port_val < 65536) {
                server_port = port_val;
            } else {
                mysyslog(LOG_LEVEL_WARNING, "Некорректное значение порта в конфигурации: %s", value);
            }
        } else if (strcmp(key, "protocol") == 0) {
            // поддерживаем значения "tcp" или "udp"
            for (char *p = value; *p; ++p) *p = tolower((unsigned char)*p);
            if (strcmp(value, "tcp") == 0 || strcmp(value, "stream") == 0) {
                use_tcp = 1;
            } else if (strcmp(value, "udp") == 0 || strcmp(value, "dgram") == 0) {
                use_tcp = 0;
            } else {
                mysyslog(LOG_LEVEL_WARNING, "Некорректное значение протокола в конфигурации: %s", value);
            }
        } else {
            mysyslog(LOG_LEVEL_WARNING, "Неизвестный параметр \"%s\" в конфигурационном файле", key);
        }
    }
    fclose(f);
}

/* Обработка одного запроса: выполнить команду и сформировать ответ JSON.
   request_text - строка JSON с запросом
   client_ip - строка с IP-адресом клиента (для логирования) */
static json_object* process_request(const char *request_text, const char *client_ip) {
    // Парсим JSON-запрос
    json_object *req_obj = json_tokener_parse(request_text);
    if (!req_obj) {
        mysyslog(LOG_LEVEL_WARNING, "Получен некорректный JSON-запрос от клиента %s", client_ip ? client_ip : "(неизвестно)");
        // Формируем ответ с ошибкой
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Неверный формат JSON"));
        return resp_err;
    }

    // Получаем поля "user" и "command"
    json_object *juser = NULL;
    json_object *jcmd = NULL;
    const char *user = NULL;
    const char *command = NULL;
    if (json_object_object_get_ex(req_obj, "user", &juser)) {
        user = json_object_get_string(juser);
    }
    if (json_object_object_get_ex(req_obj, "command", &jcmd)) {
        command = json_object_get_string(jcmd);
    }
    if (!user || !command) {
        mysyslog(LOG_LEVEL_WARNING, "В запросе отсутствует требуемое поле 'user' или 'command'");
        json_object_put(req_obj);
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Неверный формат запроса"));
        return resp_err;
    }

    // Проверка пользователя
    int allowed = 0;
    for (size_t i = 0; i < allowed_count; ++i) {
        if (strcmp(allowed_users[i], user) == 0) {
            allowed = 1;
            break;
        }
    }
    if (!allowed) {
        mysyslog(LOG_LEVEL_WARNING, "Запрос от запрещенного пользователя '%s' (клиент %s)", user, client_ip ? client_ip : "(неизвестно)");
        json_object_put(req_obj);
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Пользователю запрещено выполнение команд"));
        return resp_err;
    }

    mysyslog(LOG_LEVEL_INFO, "Выполнение команды от пользователя '%s'%s: %s", 
             user,
             (client_ip ? ", адрес клиента " : ""),
             (client_ip ? client_ip : ""),
             command);

    // Подготовка временных файлов для вывода
    char tmpOut[] = "/tmp/myrpc_stdoutXXXXXX";
    char tmpErr[] = "/tmp/myrpc_stderrXXXXXX";
    int fd_out = mkstemp(tmpOut);
    if (fd_out < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось создать временный файл для stdout: %s", strerror(errno));
        json_object_put(req_obj);
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Внутренняя ошибка сервера (stdout)"));
        return resp_err;
    }
    int fd_err = mkstemp(tmpErr);
    if (fd_err < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось создать временный файл для stderr: %s", strerror(errno));
        close(fd_out);
        unlink(tmpOut);
        json_object_put(req_obj);
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Внутренняя ошибка сервера (stderr)"));
        return resp_err;
    }
    // Закрываем файлы, чтобы shell мог их использовать
    close(fd_out);
    close(fd_err);

    // Формируем команду с перенаправлением вывода
    size_t cmd_len = strlen(command);
    // Дополнительное место: для ' > tmpOut 2> tmpErr' и кавычек - примерно strlen(tmpOut)+strlen(tmpErr)+10
    size_t buf_size = cmd_len + strlen(tmpOut) + strlen(tmpErr) + 20;
    char *shell_cmd = malloc(buf_size);
    if (!shell_cmd) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка выделения памяти для команды");
        unlink(tmpOut);
        unlink(tmpErr);
        json_object_put(req_obj);
        json_object *resp_err = json_object_new_object();
        json_object_object_add(resp_err, "error", json_object_new_string("Внутренняя ошибка сервера (alloc)"));
        return resp_err;
    }
    // Используем shell для выполнения команды
    snprintf(shell_cmd, buf_size, "%s > %s 2> %s", command, tmpOut, tmpErr);

    // Выполняем команду
    int sys_ret = system(shell_cmd);
    free(shell_cmd);
    json_object_put(req_obj); // больше не нужен входной JSON
    // Подготовка JSON ответа
    json_object *resp_obj = json_object_new_object();
    if (sys_ret == -1) {
        // Ошибка запуска shell
        mysyslog(LOG_LEVEL_ERROR, "Не удалось запустить shell для выполнения команды");
        json_object_object_add(resp_obj, "error", json_object_new_string("Ошибка запуска команды"));
    } else {
        // Определяем код возврата
        int exit_code = -1;
        if (WIFEXITED(sys_ret)) {
            exit_code = WEXITSTATUS(sys_ret);
        } else if (WIFSIGNALED(sys_ret)) {
            // если процесс был завершен сигналом
            exit_code = 128 + WTERMSIG(sys_ret);
        }
        // Читаем содержимое временных файлов
        FILE *f_out = fopen(tmpOut, "rb");
        FILE *f_err = fopen(tmpErr, "rb");
        if (!f_out || !f_err) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка открытия временных файлов результатов");
            if (f_out) fclose(f_out);
            if (f_err) fclose(f_err);
            json_object_object_add(resp_obj, "error", json_object_new_string("Ошибка чтения результатов команды"));
        } else {
            // Получаем размер stdout
            struct stat st;
            if (stat(tmpOut, &st) == 0) {
                long fsize = st.st_size;
                char *out_buf = (char*)malloc(fsize + 1);
                if (out_buf) {
                    size_t readn = fread(out_buf, 1, fsize, f_out);
                    out_buf[readn] = '\0';
                    json_object_object_add(resp_obj, "stdout", json_object_new_string(out_buf));
                    free(out_buf);
                } else {
                    json_object_object_add(resp_obj, "stdout", json_object_new_string(""));
                    mysyslog(LOG_LEVEL_ERROR, "Недостаточно памяти для чтения stdout результата");
                }
            } else {
                json_object_object_add(resp_obj, "stdout", json_object_new_string(""));
            }
            // Получаем размер stderr
            if (stat(tmpErr, &st) == 0) {
                long fsize = st.st_size;
                char *err_buf = (char*)malloc(fsize + 1);
                if (err_buf) {
                    size_t readn = fread(err_buf, 1, fsize, f_err);
                    err_buf[readn] = '\0';
                    json_object_object_add(resp_obj, "stderr", json_object_new_string(err_buf));
                    free(err_buf);
                } else {
                    json_object_object_add(resp_obj, "stderr", json_object_new_string(""));
                    mysyslog(LOG_LEVEL_ERROR, "Недостаточно памяти для чтения stderr результата");
                }
            } else {
                json_object_object_add(resp_obj, "stderr", json_object_new_string(""));
            }
            // Добавляем код возврата
            json_object_object_add(resp_obj, "exit_code", json_object_new_int(exit_code));
            fclose(f_out);
            fclose(f_err);
        }
    }
    // Удаляем временные файлы
    unlink(tmpOut);
    unlink(tmpErr);

    return resp_obj;
}

/* Основной цикл работы сервера в TCP-режиме */
static int run_tcp_server() {
    int sock_listen;
    struct sockaddr_in srv_addr;
    sock_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_listen < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось создать сокет: %s", strerror(errno));
        return -1;
    }
    // Повторно использовать порт сразу после закрытия
    int opt = 1;
    setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(server_port);

    if (bind(sock_listen, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось привязать сокет к порту %d: %s", server_port, strerror(errno));
        close(sock_listen);
        return -1;
    }
    if (listen(sock_listen, 5) < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка вызова listen: %s", strerror(errno));
        close(sock_listen);
        return -1;
    }

    mysyslog(LOG_LEVEL_INFO, "Сервер ожидает TCP-соединения на порту %d", server_port);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int sock_client = accept(sock_listen, (struct sockaddr*)&cli_addr, &cli_len);
        if (sock_client < 0) {
            if (errno == EINTR) continue; // прервано сигналом, повторяем
            mysyslog(LOG_LEVEL_ERROR, "Ошибка при принятии соединения: %s", strerror(errno));
            break;
        }
        // Получаем строку с адресом клиента
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(cli_addr.sin_port);
        mysyslog(LOG_LEVEL_INFO, "Клиент %s:%d подключен", client_ip, client_port);

        // Чтение запроса
        char *req_buffer = NULL;
        size_t req_len = 0;
        char tmp_buf[1024];
        ssize_t n;
        // Читаем до закрытия сокета клиентом
        while ((n = recv(sock_client, tmp_buf, sizeof(tmp_buf), 0)) > 0) {
            // Расширяем буфер запроса
            char *newbuf = realloc(req_buffer, req_len + n + 1);
            if (!newbuf) {
                mysyslog(LOG_LEVEL_ERROR, "Недостаточно памяти для буфера запроса");
                free(req_buffer);
                req_buffer = NULL;
                break;
            }
            req_buffer = newbuf;
            memcpy(req_buffer + req_len, tmp_buf, n);
            req_len += n;
            req_buffer[req_len] = '\0';
        }
        if (n < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка при чтении запроса от %s:%d: %s", client_ip, client_port, strerror(errno));
            close(sock_client);
            free(req_buffer);
            continue;
        }
        if (!req_buffer) {
            // Если буфер пустой (например, клиент отключился сразу)
            mysyslog(LOG_LEVEL_WARNING, "Пустой запрос от %s:%d", client_ip, client_port);
            close(sock_client);
            continue;
        }

        // Обрабатываем запрос
        json_object *resp = process_request(req_buffer, client_ip);
        free(req_buffer);
        // Преобразуем JSON-объект ответа в строку
        const char *resp_str = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
        size_t resp_len = strlen(resp_str);
        // Отправляем ответ клиенту
        ssize_t sent = 0;
        while ((size_t)sent < resp_len) {
            ssize_t m = send(sock_client, resp_str + sent, resp_len - sent, 0);
            if (m < 0) {
                mysyslog(LOG_LEVEL_ERROR, "Ошибка при отправке ответа клиенту %s:%d: %s", client_ip, client_port, strerror(errno));
                break;
            }
            sent += m;
        }
        // Завершаем соединение
        close(sock_client);
        json_object_put(resp);
        mysyslog(LOG_LEVEL_INFO, "Соединение с клиентом %s:%d закрыто", client_ip, client_port);
    }

    close(sock_listen);
    return 0;
}

/* Основной цикл работы сервера в UDP-режиме */
static int run_udp_server() {
    int sockfd;
    struct sockaddr_in srv_addr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось создать UDP-сокет: %s", strerror(errno));
        return -1;
    }
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(server_port);

    if (bind(sockfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Не удалось привязать UDP-сокет к порту %d: %s", server_port, strerror(errno));
        close(sockfd);
        return -1;
    }
    mysyslog(LOG_LEVEL_INFO, "Сервер ожидает UDP-запросы на порту %d", server_port);
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        char buffer[65536];
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&cli_addr, &cli_len);
        if (n < 0) {
            if (errno == EINTR) continue;
            mysyslog(LOG_LEVEL_ERROR, "Ошибка при получении UDP: %s", strerror(errno));
            break;
        }
        buffer[n] = '\0';
        // Получаем адрес клиента
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(cli_addr.sin_port);
        mysyslog(LOG_LEVEL_INFO, "Получен UDP-запрос от %s:%d (%zd байт)", client_ip, client_port, n);
        // Обработка запроса
        json_object *resp = process_request(buffer, client_ip);
        const char *resp_str = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
        size_t resp_len = strlen(resp_str);
        // Отправляем ответ
        ssize_t sent = sendto(sockfd, resp_str, resp_len, 0, (struct sockaddr*)&cli_addr, cli_len);
        if (sent < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка отправки UDP-ответа клиенту %s:%d: %s", client_ip, client_port, strerror(errno));
        } else {
            mysyslog(LOG_LEVEL_INFO, "Ответ отправлен клиенту %s:%d (байт: %zd)", client_ip, client_port, sent);
        }
        json_object_put(resp);
    }
    close(sockfd);
    return 0;
}

int main(int argc, char *argv[]) {
    // Инициализация системы логирования
    mysyslog_init("myRPC-server");

    // Игнорируем сигнал SIGPIPE, чтобы не завершаться при разрыве соединений
    signal(SIGPIPE, SIG_IGN);
    // Загрузка конфигурации и списка разрешенных пользователей
    const char *conf_path1 = "myRPC.conf";
    const char *conf_path2 = "/etc/myRPC.conf";
    FILE *fconf = fopen(conf_path1, "r");
    if (fconf) {
        fclose(fconf);
        load_config(conf_path1);
    } else {
        fconf = fopen(conf_path2, "r");
        if (fconf) {
            fclose(fconf);
            load_config(conf_path2);
        } else {
            mysyslog(LOG_LEVEL_WARNING, "Конфигурационный файл не найден, используются значения по умолчанию.");
        }
    }
    // Разбор аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Использование: %s [--port PORT] [--stream|--dgram]\n", argv[0]);
                mysyslog_close();
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--stream") == 0 || strcmp(argv[i], "-t") == 0) {
            use_tcp = 1;
        } else if (strcmp(argv[i], "--dgram") == 0 || strcmp(argv[i], "-u") == 0) {
            use_tcp = 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Использование: %s [--port PORT] [--stream|--dgram]\n", argv[0]);
            printf("Параметры:\n");
            printf("  --port, -p    Номер порта для слушания (по умолчанию %d или из конфигурации)\n", server_port);
            printf("  --stream, -t  Режим TCP (SOCK_STREAM)\n");
            printf("  --dgram, -u   Режим UDP (SOCK_DGRAM)\n");
            mysyslog_close();
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Неизвестный параметр: %s\n", argv[i]);
            fprintf(stderr, "Используйте --help для справки.\n");
            mysyslog_close();
            return EXIT_FAILURE;
        }
    }
    // Загрузка списка разрешённых пользователей
    const char *users_path1 = "users.conf";
    const char *users_path2 = "/etc/users.conf";
    if (load_allowed_users(users_path1) < 0) {
        if (load_allowed_users(users_path2) < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Не удалось открыть файл списка пользователей '%s' или '%s'", users_path1, users_path2);
            mysyslog_close();
            return EXIT_FAILURE;
        }
    }
    if (allowed_count == 0) {
        mysyslog(LOG_LEVEL_WARNING, "Внимание: список разрешённых пользователей пуст. Ни один пользователь не сможет выполнять команды.");
    }
    // Выводим информацию о выбранном режиме
    mysyslog(LOG_LEVEL_INFO, "Запуск сервера (порт %d, режим %s)", server_port, use_tcp ? "TCP" : "UDP");

    // Запускаем соответствующий режим сервера
    int result;
    if (use_tcp) {
        result = run_tcp_server();
    } else {
        result = run_udp_server();
    }

    // Освобождаем список пользователей
    free_allowed_users();
    // Завершаем систему логирования
    mysyslog_close();
    return (result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
