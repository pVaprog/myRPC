#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pwd.h>

#include <json-c/json.h>
#include "../libmysyslog/mysyslog.h"

int main(int argc, char *argv[]) {
    mysyslog_init("myRPC-client");

    const char *server_host = NULL;
    int server_port = 0;
    const char *command = NULL;
    int use_tcp = 1;  // 1 = TCP, 0 = UDP

    // Разбор аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                server_host = argv[++i];
            } else {
                fprintf(stderr, "Использование: %s --host HOST --port PORT --command CMD [--stream|--dgram]\n", argv[0]);
                mysyslog_close();
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Использование: %s --host HOST --port PORT --command CMD [--stream|--dgram]\n", argv[0]);
                mysyslog_close();
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--command") == 0 || strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                command = argv[++i];
            } else {
                fprintf(stderr, "Использование: %s --host HOST --port PORT --command CMD [--stream|--dgram]\n", argv[0]);
                mysyslog_close();
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--stream") == 0 || strcmp(argv[i], "-t") == 0) {
            use_tcp = 1;
        } else if (strcmp(argv[i], "--dgram") == 0 || strcmp(argv[i], "-u") == 0) {
            use_tcp = 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Использование: %s --host HOST --port PORT --command CMD [--stream|--dgram]\n", argv[0]);
            printf("Параметры:\n");
            printf("  --host       Адрес сервера (IP или имя хоста)\n");
            printf("  --port, -p   Номер порта сервера\n");
            printf("  --command, -c Команда для выполнения в кавычках\n");
            printf("  --stream, -t Использовать TCP (SOCK_STREAM)\n");
            printf("  --dgram, -u  Использовать UDP (SOCK_DGRAM)\n");
            mysyslog_close();
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Неизвестный параметр: %s\n", argv[i]);
            fprintf(stderr, "Используйте --help для справки.\n");
            mysyslog_close();
            return EXIT_FAILURE;
        }
    }

    // Проверка обязательных параметров
    if (!server_host) {
        server_host = "127.0.0.1";
        mysyslog(LOG_LEVEL_INFO, "Хост не указан, используем %s по умолчанию", server_host);
    }
    if (server_port == 0) {
        server_port = 12345;
        mysyslog(LOG_LEVEL_INFO, "Порт не указан, используем %d по умолчанию", server_port);
    }
    if (!command) {
        fprintf(stderr, "Ошибка: не указана команда для выполнения.\n");
        fprintf(stderr, "Используйте --help для справки.\n");
        mysyslog_close();
        return EXIT_FAILURE;
    }

    // Создание сокета и подключение/настройка
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = use_tcp ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_protocol = use_tcp ? IPPROTO_TCP : IPPROTO_UDP;
    struct addrinfo *res = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", server_port);
    int gai = getaddrinfo(server_host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка: не удалось преобразовать адрес %s: %s", server_host, gai_strerror(gai));
        mysyslog_close();
        return EXIT_FAILURE;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка: не удалось создать сокет: %s", strerror(errno));
        freeaddrinfo(res);
        mysyslog_close();
        return EXIT_FAILURE;
    }

    if (use_tcp) {
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка: не удалось подключиться к %s:%d: %s", server_host, server_port, strerror(errno));
            freeaddrinfo(res);
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
    } else {
        // Для UDP "подключаем" сокет к удаленному адресу (устанавливаем адрес по умолчанию)
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка: не удалось установить UDP-связь с %s:%d: %s", server_host, server_port, strerror(errno));
            freeaddrinfo(res);
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
        // Устанавливаем таймаут на получение, чтобы не зависнуть, например, 5 секунд
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    freeaddrinfo(res);

    // Получаем имя текущего пользователя
    const char *user = getenv("USER");
    if (!user) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            user = pw->pw_name;
        } else {
            user = "unknown";
        }
    }

    // Формируем JSON-запрос
    json_object *req_obj = json_object_new_object();
    json_object_object_add(req_obj, "user", json_object_new_string(user));
    json_object_object_add(req_obj, "command", json_object_new_string(command));
    const char *req_str = json_object_to_json_string_ext(req_obj, JSON_C_TO_STRING_PLAIN);
    size_t req_len = strlen(req_str);

    // Отправка запроса
    size_t sent_total = 0;
    while (sent_total < req_len) {
        ssize_t n = send(sock, req_str + sent_total, req_len - sent_total, 0);
        if (n < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка при отправке данных на сервер: %s", strerror(errno));
            json_object_put(req_obj);
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
        sent_total += n;
    }
    json_object_put(req_obj); // уже отправили, можно освободить объект

    if (use_tcp) {
        // Для TCP сигнализируем, что больше данных не будет
        shutdown(sock, SHUT_WR);
    }

    // Чтение ответа
    char *resp_buffer = NULL;
    size_t resp_size = 0;
    if (use_tcp) {
        // Читаем до закрытия соединения сервером
        char tmp[1024];
        ssize_t n;
        while ((n = recv(sock, tmp, sizeof(tmp), 0)) > 0) {
            char *newbuf = realloc(resp_buffer, resp_size + n + 1);
            if (!newbuf) {
                mysyslog(LOG_LEVEL_ERROR, "Ошибка: недостаточно памяти для буфера ответа");
                free(resp_buffer);
                close(sock);
                mysyslog_close();
                return EXIT_FAILURE;
            }
            resp_buffer = newbuf;
            memcpy(resp_buffer + resp_size, tmp, n);
            resp_size += n;
            resp_buffer[resp_size] = '\0';
        }
        if (n < 0) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка при получении данных от сервера: %s", strerror(errno));
            free(resp_buffer);
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
    } else {
        // Для UDP считаем, что ответ придёт одним датаграммой
        resp_buffer = malloc(65536);
        if (!resp_buffer) {
            mysyslog(LOG_LEVEL_ERROR, "Ошибка: недостаточно памяти для буфера ответа");
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
        ssize_t n = recv(sock, resp_buffer, 65536, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                mysyslog(LOG_LEVEL_ERROR, "Ошибка: нет ответа от сервера (таймаут)");
            } else {
                mysyslog(LOG_LEVEL_ERROR, "Ошибка при получении ответа от сервера: %s", strerror(errno));
            }
            free(resp_buffer);
            close(sock);
            mysyslog_close();
            return EXIT_FAILURE;
        }
        resp_size = n;
        resp_buffer[resp_size] = '\0';
    }
    close(sock);

    if (!resp_buffer) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка: ответ от сервера отсутствует или пустой");
        mysyslog_close();
        return EXIT_FAILURE;
    }

    // Разбор JSON-ответа
    json_object *resp_obj = json_tokener_parse(resp_buffer);
    free(resp_buffer);
    if (!resp_obj) {
        mysyslog(LOG_LEVEL_ERROR, "Ошибка: получен некорректный JSON-ответ от сервера");
        mysyslog_close();
        return EXIT_FAILURE;
    }

    json_object *jerr = NULL;
    json_object *jout = NULL;
    json_object *jerrout = NULL;
    json_object *jexit = NULL;
    if (json_object_object_get_ex(resp_obj, "error", &jerr)) {
        // Есть поле error - выводим сообщение об ошибке
        const char *err_msg = json_object_get_string(jerr);
        if (err_msg) {
            fprintf(stderr, "%s\n", err_msg);
        } else {
            fprintf(stderr, "Неизвестная ошибка сервера\n");
        }
        json_object_put(resp_obj);
        mysyslog_close();
        return EXIT_FAILURE;
    }

    // Получаем поля stdout, stderr, exit_code
    if (json_object_object_get_ex(resp_obj, "stdout", &jout)) {
        const char *out_str = json_object_get_string(jout);
        if (out_str && strlen(out_str) > 0) {
            // Выводим результат stdout на stdout
            fprintf(stdout, "%s", out_str);
            fflush(stdout);
        }
    }
    if (json_object_object_get_ex(resp_obj, "stderr", &jerrout)) {
        const char *err_str = json_object_get_string(jerrout);
        if (err_str && strlen(err_str) > 0) {
            // Выводим stderr-вывод удалённой команды на stderr
            fprintf(stderr, "%s", err_str);
            fflush(stderr);
        }
    }
    int exit_code = 0;
    if (json_object_object_get_ex(resp_obj, "exit_code", &jexit)) {
        exit_code = json_object_get_int(jexit);
    }
    json_object_put(resp_obj);
    mysyslog_close();
    return exit_code;
}
