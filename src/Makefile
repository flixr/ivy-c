#
#	Ivy, C interface
#
#	Copyright (C) 1997-2002
#	Centre d'Études de la Navigation Aérienne
#
# 	Makefile
#
#	Authors: François-Régis Colin <fcolin@cena.fr>
# 		 Stéphane Chatty <chatty@cena.fr>
# 		 Yannick Jestin <jestin@cena.fr>
#
#	Please refer to file version.h for the
#	copyright notice regarding this software
#


# change this in version.h too !!!!
MAJOR=3
MINOR=6

XTINC = -I/usr/X11R6/include
XTLIB = -L/usr/X11R6/lib -lXt -lX11 -lSM -lICE
GLIBINC = `pkg-config --cflags glib-2.0`
GLIBLIB = `pkg-config --libs glib-2.0`
GLUTINC = -I/usr/include -I.
GLUTLIB = -L. -lglut -lGLU -lGL -lX11 -lXmu 
# use these if you want standard Tcl ( 8.3 on debian woody ... )
TCLINCL	= -I/usr/include/tcl8.3
TCLLIB = -ltcl8.3
#TCLINCL	= -I/usr/include/tcl8.4
#TCLLIB = -ltcl8.4
# use these if you want to use Aqua Tcl on macOSX
#TCLINC	= -I/Library/Frameworks/Tcl.framework/Headers
#TCLLIB = -framework Tcl
PCREINC = `pcre-config --cflags`
PCRELIB = `pcre-config --libs`
#PCREOBJ = `pcre-config --prefix`/lib/libpcre.a
PCREOBJ = 

EXTRALIB=
#EXTRALIB=-lsocket -lnsl # for solaris

CC=gcc
LIBTOOL=ar q # linux and solaris
#LIBTOOL=libtool -static -o

#REGEXP = -DGNU_REGEXP  # deprecated !
REGEXP = -DUSE_PCRE_REGEX 
# on activeTCL , set #define CHANNEL to null, and add ivyloop.o in the ivytcl target,
# see below
CHANNEL = -DTCL_CHANNEL_INTEGRATION 
CFLAGS = -g -Wall

OBJ = ivyloop.o timer.o ivysocket.o ivy.o 
GOBJ = ivyloop.o timer.o ivysocket.o givy.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o
GLIBOBJ = ivyglibloop.o ivysocket.o ivy.o
GLUTOBJ = ivyglutloop.o ivysocket.o ivy.o
TCLOBJ = ivytcl.o  timer.o ivysocket.o givy.o
# WINDOWS add ivyloop.o if TCL_CHANNEL_INTEGRATION is not set
TARGETS = ivyprobe ivyglibprobe ivyxtprobe 
TARGETLIBS=libivy.so.$(MAJOR).$(MINOR) libgivy.so.$(MAJOR).$(MINOR) libxtivy.so.$(MAJOR).$(MINOR) libglibivy.so.$(MAJOR).$(MINOR) libtclivy.so.$(MAJOR).$(MINOR)
# not yet need Modified Glut ivyglutprobe

.c.o:
	$(CC) $(CFLAGS) -c $*.c

all: static-libs commands shared-libs

static-libs: libivy.a libgivy.a libxtivy.a libglibivy.a libtclivy.a 
# not yet need Modified Glut libglutivy.a

shared-libs: $(TARGETLIBS)
# not yet need Modified Glut libglutivy.so.$(MAJOR).$(MINOR)

commands: $(TARGETS)

ivytcl.o: ivytcl.c
	$(CC) -c $(CFLAGS) $(TCLINCL) $(CHANNEL) ivytcl.c

ivy.o: ivy.c
	$(CC) -c $(CFLAGS) $(REGEXP) $(PCREINC) ivy.c

givy.o: ivy.c
	$(CC) -c $(CFLAGS) -o givy.o ivy.c

ivyglutloop.o: ivyglutloop.c ivyglutloop.h
	$(CC) -c $(CFLAGS) $(GLUTINC) ivyglutloop.c

ivyglibloop.o: ivyglibloop.c ivyglibloop.h
	$(CC) -c $(CFLAGS) $(GLIBINC) ivyglibloop.c

ivyprobe: ivyprobe.o
	$(CC)  $(CFLAGS) -o $@ ivyprobe.o -L. -livy $(PCRELIB) $(EXTRALIB)

ivyprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(REGEXP) -c ivyprobe.c -o $@

ivyxtprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(REGEXP) -DXTMAINLOOP -c ivyprobe.c -o $@ $(XTINC)

ivyxtprobe: ivyxtprobe.o libxtivy.a
	$(CC) -o $@ ivyxtprobe.o -L. $(XTLIB) -lxtivy  $(PCRELIB) $(EXTRALIB)

ivyglibprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(REGEXP) -DGLIBMAINLOOP -c ivyprobe.c -o ivyglibprobe.o $(GLIBINC)

ivyglibprobe: ivyglibprobe.o libglibivy.a
	$(CC) -o ivyglibprobe ivyglibprobe.o -L. $(GLIBLIB) -lglibivy $(PCRELIB) $(EXTRALIB)

ivyglutprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(REGEXP) -DGLUTMAINLOOP -c ivyprobe.c -o ivyglutprobe.o $(GLUTINC)

ivyglutprobe: ivyglutprobe.o libglutivy.a
	$(CC) -o ivyglutprobe ivyglutprobe.o -L. $(GLUTLIB) -lglutivy $(PCRELIB) $(EXTRALIB)

libivy.a: $(OBJ)
	rm -f $@
	$(LIBTOOL) $@ $(OBJ)

libgivy.a: $(GOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GOBJ)

libxtivy.a: $(XTOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(XTOBJ)

libglibivy.a: $(GLIBOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GLIBOBJ)

libglutivy.a: $(GLUTOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(GLUTOBJ)

libtclivy.a: $(TCLOBJ)
	rm -f $@
	$(LIBTOOL) $@ $(TCLOBJ) $(PCREOBJ)
# TODO this PCREOBJ is a dirty hack

libivy.so.$(MAJOR).$(MINOR): $(OBJ)
	$(CC) -shared -Wl,-soname,libivy.so.$(MAJOR) -o $@ $(OBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libivy.so.$(MAJOR) -o $@ $(OBJ)		#solaris
#	libtool -dynamic -o $@ $(OBJ) $(PCRELIB) -lc

libgivy.so.$(MAJOR).$(MINOR): $(GOBJ)
	$(CC) -shared -Wl,-soname,libgivy.so.$(MAJOR) -o $@ $(GOBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libgivy.so.$(MAJOR) -o $@ $(GOBJ) #solaris
#	libtool -dynamic -o $@ $(GOBJ) $(PCRELIB) -lc

libxtivy.so.$(MAJOR).$(MINOR): $(XTOBJ)
	$(CC) -shared -Wl,-soname,libxtivy.so.$(MAJOR) -o $@ $(XTOBJ) $(XTLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libxtivy.so.$(MAJOR) -o $@ $(XTOBJ) $(XTLIB) #solaris
#	libtool -dynamic -o $@ $(XTOBJ) $(XTLIB) $(PCRELIB) -lc

libglibivy.so.$(MAJOR).$(MINOR): $(GLIBOBJ)
	$(CC) -shared -Wl,-soname,libglibivy.so.$(MAJOR) -o $@ $(GLIBOBJ) $(GLIBLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libglibivy.so.$(MAJOR) -o $@ $(GLIBOBJ) $(GLIBLIB) #solaris
#	libtool -dynamic -o $@ $(GLIBOBJ) $(GLIBLIB) $(PCRELIB) -lc

libglutivy.so.$(MAJOR).$(MINOR): $(GLIBOBJ)
	$(CC) -shared -Wl,-soname,libglutivy.so.$(MAJOR) -o $@ $(GLUTOBJ) $(GLUTLIB) $(PCRELIB)
#	$(CC) -G -Wl,-h,libglutivy.so.$(MAJOR) -o $@ $(GLUTOBJ) $(GLUTLIB) #	solaris
#	libtool -dynamic -o $@ $(GLUTOBJ) $(GLUTLIB) $(PCRELIB) -lc

libtclivy.so.$(MAJOR).$(MINOR): $(TCLOBJ)
	$(CC) -shared -Wl,-soname,libtclivy.so.$(MAJOR) -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) $(PCRELIB)
#	$(CC) -G -Wl,-h,libtclivy.so.$(MAJOR) -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) # solaris
#	libtool -dynamic -o $@ $(TCLOBJ) $(TCLLIB) $(PCREOBJ) $(PCRELIB) -lc

clean:
	-rm -f $(TARGETS) $(TARGETLIBS) *.o *.a *.so *.so.* *~

installlibs: static-libs shared-libs
	test -d $(PREFIX)/usr/lib || mkdirhier $(PREFIX)/usr/lib
	test -d $(PREFIX)/usr/X11R6/lib || mkdirhier $(PREFIX)/usr/X11R6/lib
	test -d $(PREFIX)/usr/include || mkdirhier $(PREFIX)/usr/include
	install -m644 libivy.a $(PREFIX)/usr/lib
	install -m644 libgivy.a $(PREFIX)/usr/lib
	install -m644 libxtivy.a $(PREFIX)/usr/X11R6/lib
	install -m644 libtclivy.a $(PREFIX)/usr/lib
	install -m644 libglibivy.a $(PREFIX)/usr/lib

	install -m644 libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib
	install -m644 libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib
	install -m644 libglibivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib

	install -m644 *.h $(PREFIX)/usr/include

installliblinks: installlibs
	ln -s /usr/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libtclivy.so
	ln -s /usr/lib/libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libivy.so
	ln -s /usr/lib/libivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libivy.so.$(MAJOR)
	ln -s /usr/lib/libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libgivy.so
	ln -s /usr/lib/libgivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/lib/libgivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libxtivy.so
	ln -s /usr/X11R6/lib/libxtivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libxtivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libglibivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libglibivy.so
	ln -s /usr/X11R6/lib/libglibivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libglibivy.so.$(MAJOR)
	ln -s /usr/X11R6/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libtclivy.so
	ln -s /usr/X11R6/lib/libtclivy.so.$(MAJOR).$(MINOR) $(PREFIX)/usr/X11R6/lib/libtclivy.so.$(MAJOR)

installbins: commands
	test -d $(PREFIX)/usr/bin || mkdirhier $(PREFIX)/usr/bin
	test -d $(PREFIX)/usr/X11R6/bin || mkdirhier $(PREFIX)/usr/X11R6/bin
	install -m755 ivyprobe $(PREFIX)/usr/bin
	#install -m755 ivyxtprobe $(PREFIX)/usr/X11R6/bin
	#install -m755 ivyglibprobe $(PREFIX)/usr/bin
	#install -m755 ivyglutprobe $(PREFIX)/usr/X11R6/bin

installdocs:
	test -d $(PREFIX)/usr/share/doc/ivy-c || mkdirhier $(PREFIX)/usr/share/doc/ivy-c
	for f in `find  ../doc/*.html -type f -maxdepth 1`; do \
                install -m 644 $$f $(PREFIX)/usr/share/doc/ivy-c; \
        done
	test -d $(PREFIX)/usr/man/man1 || mkdirhier $(PREFIX)/usr/man/man1
	for f in `find  ../doc/*.1 -type f -maxdepth 1`; do \
                install -m 644 $$f $(PREFIX)/usr/man/man1; \
        done
	test -d $(PREFIX)/usr/share/doc/ivy-c/examples || mkdirhier $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 644 version.h $(PREFIX)/usr/share/doc/ivy-c/copyright
	install -m 644 ../examples/gtkIvyButton.c $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 644 ../examples/motifButtonIvy.c $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 644 ../examples/testUnbind.c $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 766 ../examples/Test.tcl $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 766 ../examples/button.tk $(PREFIX)/usr/share/doc/ivy-c/examples
	install -m 766 ../examples/unBind.tcl $(PREFIX)/usr/share/doc/ivy-c/examples

install: installlibs installbins installliblinks installdocs

rpm::
	/usr/bin/rpmize

