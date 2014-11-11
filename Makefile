LDLIBS=-lcurses
CFLAGS=-Wall -Wextra -Werror -O2
PREFIX=/usr/local
INSTALL=install -Ds

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: rover
	$(INSTALL) rover $(DESTDIR)$(PREFIX)/bin/rover

.PHONE: clean
clean:
	$(RM) rover
