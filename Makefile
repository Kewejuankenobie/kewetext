CC = gcc
CFLAGS = -Wall -Wextra
VPATH = build

all: build/bin/kewetext


build/bin/kewetext: build/main.o build/stack.o build/configuration.o
	if [ ! -d "build/bin" ]; then mkdir -p build/bin; fi
	$(CC) $? -o $@
	@echo "Finished Making Kewetext"

build/main.o: main.c stack.h configuration.h
	if [ ! -d "build" ]; then mkdir build; fi
	$(CC) -c main.c $(CFLAGS) -o $@

build/configuration.o: configuration.c configuration.h
	$(CC) -c configuration.c $(CFLAGS) -o $@

build/stack.o: stack.c stack.h
	$(CC) -c stack.c $(CFLAGS) -o $@

clean:
	rm -rvf build
.PHONY: clean

install:
	if [ ! -d "/usr/local/bin" ]; then sudo mkdir -p /usr/local/bin; fi
	sudo cp build/bin/kewetext /usr/local/bin/kewetext
	if [ ! -d "$(HOME)/.config" ]; then \
  		mkdir $(HOME)/.config; \
  	fi
	cp kewetextrc $(HOME)/.config/kewetextrc
	@echo "Successfully Installed Kewetext"
.PHONY: install

uninstall:
	sudo rm /usr/local/bin/kewetext
	@echo "Uninstalled Kewetext"
.PHONY: uninstall