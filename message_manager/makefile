P		= main
FUNC	= message_manager.o xml.o sql.o socket.o basic.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= $(P).o $(EXT)
CC		= gcc
CFLAGS	= -Wall -g -D_REENTRANT
LDFLAGS	= -I./../include `xml2-config --cflags` `mysql_config --cflags` `xml2-config --libs` `mysql_config --libs`

$(P): $(OBJ)
	$(CC) $(CFLAGS) -o $(P) $(P).o $(FUNC) $(LDFLAGS)
	
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)
