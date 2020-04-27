# islide - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c islide.c util.c
OBJ = $(SRC:.c=.o)

all: clean options islide

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


clean:
	rm -f islide $(OBJ) islide-$(VERSION).tar.gz

dist: clean
	mkdir -p islide-$(VERSION)
	cp LICENSE Makefile README arg.h config.def.h config.mk islide.1\
		drw.h util.h $(SRC)\
		islide-$(VERSION)
	tar -cf islide-$(VERSION).tar islide-$(VERSION)
	gzip islide-$(VERSION).tar
	rm -rf islide-$(VERSION)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f islide $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/islide
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < islide.1 > $(DESTDIR)$(MANPREFIX)/man1/islide.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/islide.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/islide\
		$(DESTDIR)$(MANPREFIX)/man1/islide.1\

.PHONY: all options clean dist install uninstall
