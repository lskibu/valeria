
CC?=gcc
CFlAGS?=-O2 -Wall -Wextra -D_GNU_SOURCE
output=valeria
source=$(wildcard src/*.c)

build: $(source)
	$(CC) $(source) -o $(output) $(CFLAGS)


