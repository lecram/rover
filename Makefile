CLINK = -lcurses
CFLAGS = -Wall -Wextra -Werror -std=c89
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CFLAGS += -O0 -g
else
    CFLAGS += -O2
endif

all : rover

rover : rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(CLINK)
