

# compiler setup
CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -I.

# define targets
# TARGETS= build run_vma

all: build

build: main.c vma.c vma.h
	$(CC) -g -o vma main.c vma.c $(CFLAGS)

run_vma: build
	./vma

clean:
	rm -f vma

.PHONY: all clean
