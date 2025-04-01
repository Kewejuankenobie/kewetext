CC = gcc
CFLAGS = -Wall -Wextra
VPATH = build

all: build/bin/kewetext


build/bin/kewetext: main.o stack.o configuration.o
	mkdir build/bin
	$(CC) $? -o $@
	@echo "Finished Making Kewetext"

build/main.o: main.c stack.h configuration.h
	mkdir build
	$(CC) -c main.c $(CFLAGS) -o $@

build/configuration.o: configuration.c configuration.h
	$(CC) -c configuration.c $(CFLAGS) -o $@

build/stack.o: stack.c stack.h
	$(CC) -c stack.c $(CFLAGS) -o $@

clean:
	rm -rvf build
.PHONY: clean

install:
	cp build/bin/kewetext /usr/bin/kewetext
	cp kewetextrc ~/.config/kewetextrc
.PHONY: install

uninstall:
	rm /usr/bin/kewetext
.PHONY: uninstall