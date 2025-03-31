CC = gcc
CFLAGS = -Wall -Wextra

all: kewetext

kewetext: stack.o configuration.o main.o
	$(CC) stack.o configuration.o main.o -o kewetext
	@echo "Made"

main.o: main.c stack.h configuration.h
	$(CC) -c main.c $(CFLAGS)

configuration.o: configuration.c configuration.h
	$(CC) -c configuration.c $(CFLAGS)

stack.o: stack.c stack.h
	$(CC) -c stack.c $(CFLAGS)

clean:
	rm kewetext
	rm *.o