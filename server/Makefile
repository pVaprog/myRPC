CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../libmysyslog
LDFLAGS = -L../libmysyslog
LIBS = -lmysyslog -ljson-c

TARGET = myRPC-server

all: $(TARGET)

$(TARGET): myRPC-server.o ../libmysyslog/libmysyslog.a
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) myRPC-server.o -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Бинарный файл сервера $(TARGET) успешно собран"

myRPC-server.o: myRPC-server.c
	@echo "  CC    myRPC-server.c"
	@$(CC) $(CFLAGS) -c myRPC-server.c -o myRPC-server.o

clean:
	@rm -f $(TARGET) myRPC-server.o
	@echo "Очистка серверной части завершена"
