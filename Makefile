CLIBS=-lcurses
CFLAGS=-Wall -Wextra -Werror -O2
PREFIX=/usr
INSTALL=install -Ds

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)

install: rover
	$(INSTALL) rover $(PREFIX)/bin/rover

.PHONE: clean
clean:
	$(RM) rover
