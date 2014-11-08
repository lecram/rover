CLIBS = -lcurses
CFLAGS = -Wall -Wextra -Werror -O2

all : rover

rover : rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)
