# islide - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c islide.c stest.c util.c
OBJ = $(SRC:.c=.o)

all: clean options islide stest

options:
	@echo islide build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

config.h:
	cp config.def.h $@

$(OBJ): arg.h config.h config.mk drw.h

islide: islide.o drw.o util.o
	$(CC) -o $@ islide.o drw.o util.o $(LDFLAGS)

stest: stest.o
	$(CC) -o $@ stest.o $(LDFLAGS)

clean:
	rm -f islide stest $(OBJ) islide-$(VERSION).tar.gz

dist: clean
	mkdir -p islide-$(VERSION)
	cp LICENSE Makefile README arg.h config.def.h config.mk islide.1\
		drw.h util.h stest.1 $(SRC)\
		islide-$(VERSION)
	tar -cf islide-$(VERSION).tar islide-$(VERSION)
	gzip islide-$(VERSION).tar
	rm -rf islide-$(VERSION)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f islide stest $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/islide
	chmod 755 $(DESTDIR)$(PREFIX)/bin/stest
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < islide.1 > $(DESTDIR)$(MANPREFIX)/man1/islide.1
	sed "s/VERSION/$(VERSION)/g" < stest.1 > $(DESTDIR)$(MANPREFIX)/man1/stest.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/islide.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/stest.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/islide\
		$(DESTDIR)$(PREFIX)/bin/stest\
		$(DESTDIR)$(MANPREFIX)/man1/islide.1\
		$(DESTDIR)$(MANPREFIX)/man1/stest.1

.PHONY: all options clean dist install uninstall
