LDLIBS := -lncursesw
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/man

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

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
