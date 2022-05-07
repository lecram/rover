PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
DATADIR ?= $(DATAROOTDIR)
MANDIR ?= $(DATADIR)/man

CFLAGS ?= -O2

PKG_CONFIG ?= pkg-config

CFLAGS_NCURSESW := `$(PKG_CONFIG) --cflags ncursesw`
LIBS_NCURSESW := `$(PKG_CONFIG) --libs ncursesw`

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) $(CFLAGS_NCURSESW) -o $@ $< $(LDFLAGS) $(LIBS_NCURSESW)

install: rover
	rm -f $(DESTDIR)$(BINDIR)/rover
	mkdir -p $(DESTDIR)$(BINDIR)
	cp rover $(DESTDIR)$(BINDIR)/rover
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	cp rover.1 $(DESTDIR)$(MANDIR)/man1/rover.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/rover
	rm -f $(DESTDIR)$(MANDIR)/man1/rover.1

clean:
	rm -f rover
