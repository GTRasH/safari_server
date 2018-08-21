P		= main
FUNC	= simulator.o xml.o socket.o basic.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= main.o $(EXT)
CC		= gcc
CFLAGS	= -Wall -D_REENTRANT
LDFLAGS	= -I./../include -lm `xml2-config --cflags` `xml2-config --libs`

main: $(OBJ)
	$(CC) $(CFLAGS) -o $(P) $(P).o $(FUNC) $(LDFLAGS)
	
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)
