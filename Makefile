
CC=gcc
CFLAGS=-O2 -g -ggdb
output=valeria
source=$(wildcard src/*.c)
obj=$(wildcard tmp/*.o)

build: tmp/main.o tmp/socks5.o tmp/connection.o tmp/server.o tmp/util.o
	$(CC) $(obj) -o $(output) $(CFLAGS)

tmp/main.o: src/main.c
	$(CC) -c src/main.c -o tmp/main.o $(CFLAGS)

tmp/socks5.o: src/socks5.c
	$(CC) -c src/socks5.c -o tmp/socks5.o $(CFLAGS)

tmp/connection.o: src/connection.c
	$(CC) -c src/connection.c -o tmp/connection.o $(CFLAGS)

tmp/server.o: src/server.c
	$(CC) -c src/server.c -o tmp/server.o $(CFLAGS)

tmp/util.o: src/util.c
	$(CC) -c src/util.c -o tmp/util.o $(CFLAGS)



clean:
	rm tmp/*.o
	rm valeria 
