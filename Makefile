SUBDIRS = libmysyslog client server

.PHONY: all $(SUBDIRS) clean deb

all: $(SUBDIRS)
	@echo "Сборка проекта завершена"

$(SUBDIRS):
	@$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	@rm -rf pkg
	@rm -f myrpc_*.deb
	@rm -f ../myRPC_*.build ../myRPC_*.changes ../myRPC_*.tar.gz
	@echo "Очистка проекта завершена"

deb: all
	@echo "Подготовка пакета DEB..."
	@rm -rf pkg
	@mkdir -p pkg/DEBIAN pkg/usr/bin pkg/etc/myRPC 
	@cp client/myRPC-client pkg/usr/bin/
	@cp server/myRPC-server pkg/usr/bin/
	@# Сжатие бинарных файлов (необязательно), удаляем символы отладки
	@strip pkg/usr/bin/myRPC-client pkg/usr/bin/myRPC-server 2>/dev/null || true
	@# Копирование конфигурационных файлов
	@cp myRPC.conf pkg/etc/myRPC/myRPC.conf
	@cp users.conf pkg/etc/myRPC/users.conf
	@# Копирование документации
	@# Создание control-файла
	@echo "Package: myrpc" > pkg/DEBIAN/control
	@echo "Version: 1.0-1" >> pkg/DEBIAN/control
	@echo "Section: base" >> pkg/DEBIAN/control
	@echo "Priority: optional" >> pkg/DEBIAN/control
	@echo "Architecture: amd64" >> pkg/DEBIAN/control
	@echo "Depends: libjson-c-dev" >> pkg/DEBIAN/control
	@echo "Maintainer: Maintainer <pva082005@gmail.com>" >> pkg/DEBIAN/control
	@echo "Description: myRPC - удаленное выполнение команд через JSON" >> pkg/DEBIAN/control
	@echo " myRPC предоставляет клиент-серверную систему для удаленного выполнения команд." >> pkg/DEBIAN/control
	@echo " Запросы и ответы передаются в формате JSON с использованием библиотеки json-c." >> pkg/DEBIAN/control
	# Построение деб-пакета
	@dpkg-deb --build pkg myrpc_1.0-1_amd64.deb
	@echo "DEB-пакет myrpc_1.0-1_amd64.deb создан"
