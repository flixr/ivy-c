#
#	Ivy, C interface
#
#	Copyright (C) 1997-1999
#	Centre d'Études de la Navigation Aérienne
#
# 	Makefile
#
#	Authors: François-Régis Colin <colin@cenatoulouse.dgac.fr>
# 		Stéphane Chatty <chatty@cenatoulouse.dgac.fr>
#
#	$Id$
# 
#	Please refer to file version.h for the
#	copyright notice regarding this software
#
#

XTINC = /usr/X11R6/include
XTLIB = /usr/X11R6/lib 
CC=gcc
CFLAGS =  -g -Wall
OBJ = ivyloop.o timer.o ivysocket.o ivy.o 
GOBJ = ivyloop.o timer.o ivysocket.o givy.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o
TARGETS = ivyprobe ivyxtprobe

ifneq (strip $(MAJOR),)
MAJOR=1
endif
ifneq (strip $(MINOR),)
MINOR=3
endif

.c.o:
	$(CC) -fPIC $(CFLAGS) -c $*.c

all: static-libs commands shared-libs

static-libs: libivy.a libgivy.a libxtivy.a

shared-libs: libivy.so.$(MAJOR).$(MINOR) libgivy.so.$(MAJOR).$(MINOR) libxtivy.so.$(MAJOR).$(MINOR)

commands: $(TARGETS)

givy.o: ivy.c
	$(CC) -DGNU_REGEXP -fPIC -c $(CFLAGS) -o givy.o ivy.c

ivyprobe: ivyprobe.o libivy.a
	$(CC)  $(CFLAGS) -o ivyprobe ivyprobe.o -L. -livy

ivyxtprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) -c ivyprobe.c -o ivyxtprobe.o -I$(XTINC) -DXTMAINLOOP

ivyxtprobe: ivyxtprobe.o libxtivy.a
	$(CC) -o ivyxtprobe ivyxtprobe.o -L. -L$(XTLIB) -lxtivy -lXt -lX11 -lSM -lICE

libivy.a: $(OBJ)
	rm -f $@
	ar q $@ $(OBJ)

libgivy.a: $(GOBJ)
	rm -f $@
	ar q $@ $(GOBJ)

libxtivy.a: $(XTOBJ)
	rm -f $@
	ar cq $@ $(XTOBJ)

libivy.so.$(MAJOR).$(MINOR): $(OBJ)
	$(CC) -shared -Wl,-soname,libivy.so.$(MAJOR) -o $@ $(OBJ) -lc

libgivy.so.$(MAJOR).$(MINOR): $(GOBJ)
	$(CC) -shared -Wl,-soname,libgivy.so.$(MAJOR) -o $@ $(GOBJ) -lc

libxtivy.so.$(MAJOR).$(MINOR): $(XTOBJ)
	$(CC) -shared -Wl,-soname,libxtivy.so.$(MAJOR) -o $@ \
	$(XTOBJ) -L$(XTLIB) -lXt -lX11 -lc

clean:
	-rm -f $(TARGETS) *.o *.a *.so *.so.* *~

install: all
	test -d $(PREFIX)/usr/bin || mkdirhier $(PREFIX)/usr/bin
	test -d $(PREFIX)/usr/X11R6/bin || mkdirhier $(PREFIX)/usr/X11R6/bin
	test -d $(PREFIX)/usr/lib || mkdirhier $(PREFIX)/usr/lib
	test -d $(PREFIX)/usr/X11R6/lib || mkdirhier $(PREFIX)/usr/X11R6/lib
	test -d $(PREFIX)/usr/include || mkdirhier $(PREFIX)/usr/include
	install -m755 ivyprobe $(PREFIX)/usr/bin
	install -m755 ivyxtprobe $(PREFIX)/usr/X11R6/bin

	install -m644 libivy.a $(PREFIX)/usr/lib
	install -m644 libgivy.a $(PREFIX)/usr/lib
	install -m644 libxtivy.a $(PREFIX)/usr/X11R6/lib

	install -m644 libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib
	ln -s /usr/lib/libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libivy.so
	ln -s /usr/lib/libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libgivy.so
	ln -s /usr/X11R6/lib/libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libxtivy.so

	install -m644 *.h $(PREFIX)/usr/include
rpm::
	/usr/bin/rpmize

