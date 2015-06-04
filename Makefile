CFLAGS=-D_FILE_OFFSET_BITS=64
LDLIBS=-lcurses
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
INSTALL=install -D
UNINSTALL=rm

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: rover
	$(INSTALL) rover $(DESTDIR)$(PREFIX)/bin/rover
	$(INSTALL) rover.1 $(DESTDIR)$(MANPREFIX)/man1/rover.1

uninstall: $(DESTDIR)$(PREFIX)/bin/rover
	$(UNINSTALL) $(DESTDIR)$(PREFIX)/bin/rover
	$(UNINSTALL) $(DESTDIR)$(MANPREFIX)/man1/rover.1

clean:
	$(RM) rover
