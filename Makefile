CC?=gcc
CFLAGS?=-O2 -Wall -Wextra
output=bin/valeria
source:=$(wildcard src/*.c)

build: src/main.c
    $(CC) $(source) -o $(output) $(FLAGS) 


