
CC=gcc
CFlAGS=-O2 -Wall -Wextra -lpthread
output=valeria
source=$(wildcard src/*.c)

build: $(source)
	$(CC) $(source) -o $(output) $(CFLAGS)


clean:
	rm valeria 
