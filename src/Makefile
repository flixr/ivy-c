#
# Ivy, C interface
#
# Copyright 1997-1998
# Centre d'Etudes de la Navigation Aerienne
#
# Makefile
#
# $Id$
#

XTINC = /usr/X11R6/include
XTLIB = /usr/X11R6/lib 
CC=gcc
CFLAGS =  -g
OBJ = ivyloop.o timer.o ivysocket.o ivy.o 
GOBJ = ivyloop.o timer.o ivysocket.o givy.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o

.c.o:
	$(CC) -fPIC $(CFLAGS) -c $*.c

all: static-libs commands shared-libs

static-libs: libivy.a libgivy.a libxtivy.a

shared-libs: libivy.so.$(VERSION).$(MAJOR_MINOR) libgivy.so.$(VERSION).$(MAJOR_MINOR libxtivy.so.$(VERSION).$(MAJOR_MINOR)

commands: testivy testxtivy

givy.o: ivy.c
	$(CC) -DGNU_REGEXP -fPIC -c $(CFLAGS) -o givy.o ivy.c

testivy: testivy.o libivy.a
	$(CC)  $(CFLAGS) -o testivy testivy.o -L. -livy

testxtivy.o : testivy.c
	$(CC)  $(CFLAGS) -c testivy.c -o testxtivy.o -I$(XTINC) -DXTMAINLOOP

testxtivy: testxtivy.o libxtivy.a
	$(CC) -o testxtivy testxtivy.o -L. -L$(XTLIB) -lxtivy -lXt -lX11

libivy.a: $(OBJ)
	rm -f $@
	ar q $@ $(OBJ)

libgivy.a: $(GOBJ)
	rm -f $@
	ar q $@ $(GOBJ)

libxtivy.a: $(XTOBJ)
	rm -f $@
	ar cq $@ $(XTOBJ)

libivy.so.$(VERSION).$(MAJOR_MINOR): $(OBJ)
	$(CC) -shared -Wl,-soname,libivy.so.$(VERSION) -o $@ $(OBJ) -lc

libgivy.so.$(VERSION).$(MAJOR_MINOR): $(GOBJ)
	$(CC) -shared -Wl,-soname,libgivy.so.$(VERSION) -o $@ $(GOBJ) -lc

libxtivy.so.$(VERSION).$(MAJOR_MINOR): $(XTOBJ)
	$(CC) -shared -Wl,-soname,libxtivy.so.$(VERSION) -o $@ \
	$(XTOBJ) -L$(XTLIB) -lXt -lX11 -lc

clean:
	-rm -f *.o *.a *.so *~

install: all
	install -m755 testivy $(DESTDIR)/usr/bin
	install -m755 testxtivy $(DESTDIR)/usr/X11R6/bin

	install -m644 libivy.a $(DESTDIR)/usr/lib
	install -m644 libgivy.a $(DESTDIR)/usr/lib
	install -m644 libxtivy.a $(DESTDIR)/usr/X11R6/lib

	install -m644 libivy.so.$(VERSION).$(MAJOR_MINOR) $(DESTDIR)/usr/lib
	install -m644 libgivy.so.$(VERSION).$(MAJOR_MINOR) $(DESTDIR)/usr/lib
	install -m644 libxtivy.so.$(VERSION).$(MAJOR_MINOR) $(DESTDIR)/usr/X11R6/lib
	cd $(DESTDIR)/usr/lib && ln -s libivy.so.$(VERSION).$(MAJOR_MINOR) libivy.so
	cd $(DESTDIR)/usr/lib && ln -s libgivy.so.$(VERSION).$(MAJOR_MINOR) libgivy.so
	cd $(DESTDIR)/usr/X11R6/lib && ln -s libxtivy.so.$(VERSION).$(MAJOR_MINOR) libxtivy.so

	install -m644 *.h $(DESTDIR)/usr/include
