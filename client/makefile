FUNC	= socket.o xml.o basic.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= main.o $(EXT)
CC		= gcc
CFLAGS	= -Wall -g -D_REENTRANT
LDFLAGS	= -I./../include `xml2-config --cflags` `xml2-config --libs`

main: $(OBJ)
	$(CC) $(CFLAGS) -o main main.o $(FUNC) $(LDFLAGS)
	
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)
