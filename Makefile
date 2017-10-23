CC = gcc
VAL = valgrind
CFLAGS = -w -Wall -g -Werror
COBJECTS = raw.o
SOBJECTS = duckchatserver.o

all: client server

client: $(COBJECTS) client.o
	$(CC) -o $@ $(COBJECTS) client.o

server: $(SOBJECTS) server.o
	$(CC) -o $@ $(SOBJECTS) server.o

clean:
	rm -f *.o client server
