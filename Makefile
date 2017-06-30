LDLIBS=-lncursesw
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
BINDIR=$(DESTDIR)$(PREFIX)/bin
MANDIR=$(DESTDIR)$(MANPREFIX)/man1

all: rover

rover: rover.c config.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

install: rover
	mkdir -p $(BINDIR)
	install -m755 rover $(BINDIR)/rover
	mkdir -p $(MANDIR)
	install -m644 rover.1 $(MANDIR)/rover.1

uninstall: $(DESTDIR)$(PREFIX)/bin/rover
	rm $(BINDIR)/rover
	rm $(MANDIR)/rover.1

clean:
	$(RM) rover
