P		= message_manager
OBJ		= $(P).o
CC		= gcc
IDIR	= ../include
CFLAGS	= `mysql_config --cflags`
LDFLAGS	= `mysql_config --libs`
DEPS	= data_elements.h map.h

$(P):$(OBJ)
	$(CC) $(CFLAGS) -o $(P) $(OBJ) $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $<
