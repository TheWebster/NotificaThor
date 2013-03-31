SHELL      = /bin/sh
CC         = /usr/bin/gcc
VER        = $(shell cat ../VERSION)
ifeq ($(DEBUG),1)
GLOB_FLAGS = -Wall -O0 -g -D 'VERSION="$(VER)"'
else
GLOB_FLAGS = -Wall -Os -D 'VERSION="$(VER)"'
endif

.PHONY: all clean

all: ../bin ../bin/notificathor ../bin/thor-cli

clean:
	rm *.o


# NotificaThor
THOR_SRC    = NotificaThor.c logging.c config.c com.c wins.c theme.c drawing.c utils.c
THOR_OBJ    = $(THOR_SRC:.c=.o)
THOR_LIBS   = -lrt -lxcb -lcairo -pthread
THOR_CFLAGS = 

../bin/notificathor: $(THOR_OBJ)
	$(CC) $(THOR_LIBS) -o $@ $^

$(THOR_OBJ): %.o: %.c NotificaThor.h config.h com.h wins.h theme.h drawing.h theme_symbols.h makefile
	$(CC) $(GLOB_FLAGS) $(THOR_CFLAGS) -c $<
	

../bin/thor-cli: thor-cli.c com.h
	$(CC) $(GLOB_FLAGS) -o $@ thor-cli.c

../bin:
	mkdir ../bin

