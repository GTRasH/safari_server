FUNC	= client_manager.o xml.o sql.o socket.o basic.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= main.o $(EXT)
CC		= gcc
CFLAGS	= -Wall -g -D_REENTRANT
LDFLAGS	= -I./../include `xml2-config --cflags` `mysql_config --cflags` `xml2-config --libs` `mysql_config --libs`

main: $(OBJ)
	$(CC) $(CFLAGS) -o main main.o $(FUNC) $(LDFLAGS)
	
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)
