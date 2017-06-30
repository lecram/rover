LDLIBS=-lncursesw
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
BINDIR=$(DESTDIR)$(PREFIX)/bin
MANDIR=$(DESTDIR)$(MANPREFIX)/man1

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

install: rover
	rm -f $(BINDIR)/rover
	mkdir -p $(BINDIR)
	cp rover $(BINDIR)/rover
	mkdir -p $(MANDIR)
	cp rover.1 $(MANDIR)/rover.1

uninstall:
	rm -f $(BINDIR)/rover
	rm -f $(MANDIR)/rover.1

clean:
	rm -f rover
