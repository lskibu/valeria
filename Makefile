
CC=gcc
CFLAGS=-O2 -pthread
output=valeria
source=$(wildcard src/*.c)
obj=$(wildcard tmp/*.o)

build: tmp/main.o tmp/socks5.o
	$(CC) $(obj) -o $(output) $(CFLAGS)

tmp/main.o: src/main.c
	$(CC) -c src/main.c -o tmp/main.o $(CFLAGS)

tmp/socks5.o: src/socks5.c
	$(CC) -c src/socks5.c -o tmp/socks5.o $(CFLAGS)


clean:
	rm tmp/*.o
	rm valeria 
