CLIBS = -lcurses
GNU ?= 0
ifeq ($(GNU), 1)
    CFLAGS = -Wall -Wextra -Werror -std=c89
    DEBUG ?= 0
    ifeq ($(DEBUG), 1)
        CFLAGS += -O0 -g
    else
        CFLAGS += -O2
    endif
endif

all : rover

rover : rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)
