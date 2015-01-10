LDLIBS=-lcurses
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
INSTALL=install -D

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: rover
	$(INSTALL) rover $(DESTDIR)$(PREFIX)/bin/rover
	$(INSTALL) rover.1 $(DESTDIR)$(MANPREFIX)/man1/rover.1

clean:
	$(RM) rover
