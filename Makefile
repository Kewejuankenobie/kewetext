CC = gcc
CFLAGS = -Wall -Wextra
VPATH = build

all: build/bin/kewetext


build/bin/kewetext: main.o stack.o configuration.o
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
	sudo cp build/bin/kewetext /usr/bin/kewetext
	if [ ! -d "$(HOME)/.config" ]; then \
  		mkdir $(HOME)/.config; \
  	fi
	cp kewetextrc $(HOME)/.config/kewetextrc
.PHONY: install

uninstall:
	sudo rm /usr/bin/kewetext
.PHONY: uninstall