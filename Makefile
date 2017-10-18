CC = gcc
VAL = valgrind
CFLAGS = -w -Wall -g
OBJECTS = 

all: client server

client: $(OBJECTS) client.o
	$(CC) -o $@ $(OBJECTS) client.o

server: $(OBJECTS) server.o
	$(CC) -o $@ $(OBJECTS) server.o

clean:
	rm -f *.o client server
