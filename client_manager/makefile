FUNC	= signals.o socket.o basic.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= main.o $(EXT)
CC		= gcc
CFLAGS	= -Wall -g -D_REENTRANT
LDFLAGS	= -I./../include `xml2-config --cflags` `xml2-config --libs`

main: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o main main.o $(FUNC)
	
%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $<
