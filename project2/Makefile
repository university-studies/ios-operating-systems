CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
SEMAPHORE=-lrt

all:barbers

barbers.o:barbers.c
	$(CC) $(CFLAGS) $(SEMAPHORE) -c barbers.c -o barbers.o

barbers:barbers.o
	$(CC) $(CFLAGS) $(SEMAPHORE) barbers.o -o barbers


pack:
	zip xloffa00.zip barbers.c Makefile