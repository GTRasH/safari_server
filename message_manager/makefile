P		= message_manager
FUNC	= error.o xmlNodes.o
EXT		= $(FUNC:%.o=./../func/%.o)
OBJ		= $(P).o $(EXT)
CC		= gcc
CFLAGS	= -Wall -g -D_REENTRANT
LDFLAGS	= -I./../include `xml2-config --cflags` `mysql_config --cflags` `xml2-config --libs` `mysql_config --libs`

$(P): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(P) $(P).o $(FUNC)
	
%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $<
