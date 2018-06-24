P=simulator
OBJ=
CC=gcc
CFLAGS=-I./../include/
DEPS = data_elements.h map.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(P):$(OBJ)
	$(CC) -o simulator simulator.c $(CFLAGS)