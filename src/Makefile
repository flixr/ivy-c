#
#	Ivy, C interface
#
#	Copyright (C) 1997-2002
#	Centre d'Études de la Navigation Aérienne
#
# 	Makefile
#
#	Authors: François-Régis Colin <fcolin@cena.fr>
# 		Stéphane Chatty <chatty@cena.fr>
#
#	$Id$
# 
#	Please refer to file version.h for the
#	copyright notice regarding this software
#


MAJOR=3
MINOR=4

XTINC = -I/usr/X11R6/include
XTLIB = -L/usr/X11R6/lib 
GTKINC = `gtk-config --cflags`
#GTKINC = -I/usr/include/gtk-1.2 -I/usr/include/glib-1.2 -I/usr/lib/glib/include -I/usr/X11R6/include
GTKLIB = `gtk-config --libs`
#GTKLIB = -L/usr/lib -L/usr/X11R6/lib
GLUTINC = -I/usr/include -I.
GLUTLIB = -L.
TCLINC = -I/usr/include/
TCLLIB = -ltcl

CC=gcc
CFLAGS = -g
OBJ = ivyloop.o timer.o ivysocket.o ivy.o 
GOBJ = ivyloop.o timer.o ivysocket.o givy.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o
GTKOBJ = ivygtkloop.o ivysocket.o ivy.o
GLUTOBJ = ivyglutloop.o ivysocket.o ivy.o
TCLOBJ = ivytcl.o ivysocket.o givy.o
TARGETS = ivyprobe ivygtkprobe ivyxtprobe 
# not yiet need Modified Glut ivyglutprobe

.c.o:
	$(CC) $(CFLAGS) -c $*.c

all: static-libs commands shared-libs

static-libs: libivy.a libgivy.a libxtivy.a libgtkivy.a libtclivy.a 
# not yiet need Modified Glut libglutivy.a

shared-libs: libivy.so.$(MAJOR).$(MINOR) libgivy.so.$(MAJOR).$(MINOR) libxtivy.so.$(MAJOR).$(MINOR) libgtkivy.so.$(MAJOR).$(MINOR) libtclivy.so.$(MAJOR).$(MINOR)
# not yiet need Modified Glut libglutivy.so.$(MAJOR).$(MINOR)

commands: $(TARGETS)

ivytcl.o: ivytcl.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) $(TCLINC) -o ivytcl.o ivytcl.c

givy.o: ivy.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) -o givy.o ivy.c

ivyglutloop.o: ivyglutloop.c ivyglutloop.h
	$(CC) -c $(CFLAGS) $(GLUTINC) -o ivyglutloop.o ivyglutloop.c

ivygtkloop.o: ivygtkloop.c ivygtkloop.h
	$(CC) -c $(CFLAGS) $(GTKINC) -o ivygtkloop.o ivygtkloop.c

ivyprobe: ivyprobe.o libivy.a
	$(CC)  $(CFLAGS) -o ivyprobe ivyprobe.o -L. -livy

ivyxtprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) -c ivyprobe.c -o ivyxtprobe.o $(XTINC) -DXTMAINLOOP

ivyxtprobe: ivyxtprobe.o libxtivy.a
	$(CC) -o ivyxtprobe ivyxtprobe.o -L. $(XTLIB) -lxtivy -lXt -lX11 -lSM -lICE

ivygtkprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) -c ivyprobe.c -o ivygtkprobe.o $(XTINC) -DGTKMAINLOOP

ivygtkprobe: ivygtkprobe.o libgtkivy.a
	$(CC) -o ivygtkprobe ivygtkprobe.o -L. $(GTKLIB) -lgtkivy -lgtk -lgdk -lglib -lX11 -lSM -lICE -lm

ivyglutprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) -c ivyprobe.c -o ivyglutprobe.o $(GLUTINC) -DGLUTMAINLOOP

ivyglutprobe: ivyglutprobe.o libglutivy.a
	$(CC) -o ivyglutprobe ivyglutprobe.o -L. $(GLUTLIB) -lglutivy -lglut -lGLU -lGL -lX11 -lXmu

libivy.a: $(OBJ)
	rm -f $@
	ar q $@ $(OBJ)

libgivy.a: $(GOBJ)
	rm -f $@
	ar q $@ $(GOBJ)

libxtivy.a: $(XTOBJ)
	rm -f $@
	ar cq $@ $(XTOBJ)

libgtkivy.a: $(GTKOBJ)
	rm -f $@
	ar cq $@ $(GTKOBJ)

libglutivy.a: $(GLUTOBJ)
	rm -f $@
	ar cq $@ $(GLUTOBJ)

libtclivy.a: $(TCLOBJ)
	rm -f $@
	ar cq $@ $(TCLOBJ)

libivy.so.$(MAJOR).$(MINOR): $(OBJ)
	$(CC) -shared -Wl,-soname,libivy.so.$(MAJOR) -o $@ $(OBJ) -lc

libgivy.so.$(MAJOR).$(MINOR): $(GOBJ)
	$(CC) -shared -Wl,-soname,libgivy.so.$(MAJOR) -o $@ $(GOBJ) -lc

libxtivy.so.$(MAJOR).$(MINOR): $(XTOBJ)
	$(CC) -shared -Wl,-soname,libxtivy.so.$(MAJOR) -o $@ \
	$(XTOBJ) $(XTLIB) -lXt -lX11 -lc

libgtkivy.so.$(MAJOR).$(MINOR): $(GTKOBJ)
	$(CC) -shared -Wl,-soname,libgtkivy.so.$(MAJOR) -o $@ \
	$(GTKOBJ) $(GTKLIB) -lX11 -lc

libglutivy.so.$(MAJOR).$(MINOR): $(GTKOBJ)
	$(CC) -shared -Wl,-soname,libglutivy.so.$(MAJOR) -o $@ \
	$(GLUTOBJ) $(GLUTLIB) -lX11 -lc

libtclivy.so.$(MAJOR).$(MINOR): $(TCLOBJ)
	$(CC) -shared -Wl,-soname,libtclivy.so.$(MAJOR) -o $@ \
	$(TCLOBJ) $(TCLLIB)  -lc

clean:
	-rm -f $(TARGETS) *.o *.a *.so *.so.* *~

installlibs: static-libs shared-libs
	test -d $(PREFIX)/usr/lib || mkdirhier $(PREFIX)/usr/lib
	test -d $(PREFIX)/usr/X11R6/lib || mkdirhier $(PREFIX)/usr/X11R6/lib
	test -d $(PREFIX)/usr/include || mkdirhier $(PREFIX)/usr/include
	install -m644 libivy.a $(PREFIX)/usr/lib
	install -m644 libgivy.a $(PREFIX)/usr/lib
	install -m644 libxtivy.a $(PREFIX)/usr/X11R6/lib
	install -m644 libtclivy.a $(PREFIX)/usr/lib
	install -m644 libgtkivy.a $(PREFIX)/usr/X11R6/lib

	install -m644 libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib
	install -m644 libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libgtkivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib

	install -m644 *.h $(PREFIX)/usr/include

installliblinks: installlibs
	ln -s /usr/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libtclivy.so
	ln -s /usr/lib/libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libivy.so
	ln -s /usr/lib/libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libivy.so.$(MAJOR)
	ln -s /usr/lib/libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libgivy.so
	ln -s /usr/lib/libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libgivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libxtivy.so
	ln -s /usr/X11R6/lib/libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libxtivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libgtkivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libgtkivy.so
	ln -s /usr/X11R6/lib/libgtkivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libgtkivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libtclivy.so
	ln -s /usr/X11R6/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libtclivy.so.$(MAJOR)

installbins: commands
	test -d $(PREFIX)/usr/bin || mkdirhier $(PREFIX)/usr/bin
	test -d $(PREFIX)/usr/X11R6/bin || mkdirhier $(PREFIX)/usr/X11R6/bin
	install -m755 ivyprobe $(PREFIX)/usr/bin
	install -m755 ivyxtprobe $(PREFIX)/usr/X11R6/bin
	install -m755 ivygtkprobe $(PREFIX)/usr/X11R6/bin
#	install -m755 ivyglutprobe $(PREFIX)/usr/X11R6/bin

installdocs:
	test -d $(PREFIX)/usr/doc/ivy-c || mkdirhier $(PREFIX)/usr/doc/ivy-c
	for f in `find  ../doc/*.html -type f -maxdepth 1`; do \
                install -m 644 $$f $(PREFIX)/usr/doc/ivy-c; \
        done
	test -d $(PREFIX)/usr/man/man1 || mkdirhier $(PREFIX)/usr/man/man1
	for f in `find  ../doc/*.1 -type f -maxdepth 1`; do \
                install -m 644 $$f $(PREFIX)/usr/man/man1; \
        done

install: installlibs installbins installliblinks installdocs

rpm::
	/usr/bin/rpmize

