LDLIBS=-lcurses
CFLAGS=-Wall -Wextra -Werror -O2
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
INSTALL=install -Ds

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: rover
	$(INSTALL) rover $(DESTDIR)$(PREFIX)/bin/rover
	$(INSTALL) rover.1 $(DESTDIR)$(MANPREFIX)/man1/rover.1

.PHONE: clean
clean:
	$(RM) rover
