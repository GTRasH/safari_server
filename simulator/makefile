P=simulator
OBJ=$(P).o
CC=gcc
IDIR=../include
CFLAGS=-I$(IDIR)
DEPS = data_elements.h map.h

$(P):$(OBJ)
	$(CC) -o simulator simulator.c $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $^ $(CFLAGS)
