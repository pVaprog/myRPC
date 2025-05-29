CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

LIB_NAME = libmysyslog.a

OBJS = mysyslog.o

all: $(LIB_NAME)

$(LIB_NAME): $(OBJS)
	@echo "  AR    $@"
	@ar rcs $(LIB_NAME) $(OBJS)
	@echo "Собран статический библиотечный файл $(LIB_NAME)"

mysyslog.o: mysyslog.c mysyslog.h
	@echo "  CC    mysyslog.c"
	@$(CC) $(CFLAGS) -c mysyslog.c -o mysyslog.o

clean:
	@rm -f $(OBJS) $(LIB_NAME)
	@echo "Очистка файлов библиотеки mysyslog завершена"
