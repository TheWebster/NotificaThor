SHELL        = /bin/sh
CC           = /usr/bin/gcc
GLOB_FLAGS   = -Wall -Os


all: ../bin ../bin/notificathor ../bin/thor-cli

clean:
	rm *.o

.PHONY: all clean

# NotificaThor
THOR_SRC    = NotificaThor.c logging.c config.c com.c wins.c theme.c drawing.c utils.c
THOR_OBJ    = $(THOR_SRC:.c=.o)
THOR_LIBS   = -lrt -lxcb -lcairo
THOR_CFLAGS = 

../bin/notificathor: $(THOR_OBJ)
	$(CC) $(THOR_LIBS) -o $@ $^

$(THOR_OBJ): %.o: %.c NotificaThor.h config.h com.h wins.h theme.h drawing.h theme_symbols.h makefile
	$(CC) $(GLOB_FLAGS) $(THOR_CFLAGS) -c $<
	

../bin/thor-cli: thor-cli.c com.h
	$(CC) $(GLOB_FLAGS) -o $@ thor-cli.c

../bin:
	mkdir ../bin

