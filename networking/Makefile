CC = gcc
CFLAGS = -Wall -g
LIBS = -lcrypto   # needed later for MD5 checksum

all: client server

client: client.c sham.h
	$(CC) $(CFLAGS) client.c -o client $(LIBS)

server: server.c sham.h
	$(CC) $(CFLAGS) server.c -o server $(LIBS)

clean:
	rm -f client server *.o *.txt *.log
