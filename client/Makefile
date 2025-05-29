CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../libmysyslog -D_POSIX_C_SOURCE=200112L
LDFLAGS = -L../libmysyslog
LIBS = -lmysyslog -ljson-c

TARGET = myRPC-client

all: $(TARGET)

$(TARGET): myRPC-client.o ../libmysyslog/libmysyslog.a
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) myRPC-client.o -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "Бинарный файл клиента $(TARGET) успешно собран"

myRPC-client.o: myRPC-client.c
	@echo "  CC    myRPC-client.c"
	@$(CC) $(CFLAGS) -c myRPC-client.c -o myRPC-client.o

clean:
	@rm -f $(TARGET) myRPC-client.o
	@echo "Очистка клиентской части завершена"
