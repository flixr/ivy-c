CC = gcc -g -Wall
CPP='g++'
BUGGY_DEBIAN_OPTION = -DPCRECPP_EXP_DEFN="" -DPCRECPP_EXP_DECL=""
OMPCFLAGS = $(CFLAGS) -fopenmp -DOPENMP=1
OMPLIB = -lgomp -lpthread
GLIBINC = `pkg-config --cflags glib-2.0`
GLIBLIB = `pkg-config --libs glib-2.0`
GLUTINC = -I/usr/include -I.
GLUTLIB = -L. -lglut -lGLU -lGL -lX11 -lXmu 




TARGETS = ivyprobe ivythroughput ivyperf ivyglibprobe ivyxtprobe #ivyprobe_efence 
OMP_TARGET= ivyprobe_omp ivythroughput_omp 

PCRELIB = `pcre-config --libs`
EXTRALIB= -L../src
EXTRAINC=-I../src

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) $(EXTRAINC) -c $*.c
.cpp.o:
	$(CPP) $(CFLAGS) $(EXTRAINC) $(BUGGY_DEBIAN_OPTION) -c $*.cpp


all: $(TARGETS)
omp: $(OMP_TARGET)

ivyprobe: ivyprobe.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o -L. -livy $(PCRELIB) $(EXTRALIB) 

ivythroughput: ivythroughput.o ../src/libivy.a
	$(CPP)  $(CFLAGS) $(EXTRAINC) -o $@ ivythroughput.o -L. -livy -lpcrecpp $(PCRELIB) $(EXTRALIB) 

ivyprobe_efence: ivyprobe.o ../src/libivy.a
	$(CPP)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o -L. -livy -lpcrecpp $(PCRELIB) $(EXTRALIB) -lefence



ivyprobe_omp: ivyprobe.o ../src/libivy_omp.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyprobe.o -L. -livy_omp $(PCRELIB) $(EXTRALIB) $(OMPLIB)

ivythroughput_omp: ivythroughput.o ../src/libivy_omp.a
	$(CPP)  $(CFLAGS) $(EXTRAINC) -o $@ ivythroughput.o -L. -livy_omp -lpcrecpp $(PCRELIB) $(EXTRALIB) $(OMPLIB)

ivyprobe.o : ivyprobe.c 
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -c ivyprobe.c -o $@

ivyperf: ivyperf.o ../src/libivy.a
	$(CC)  $(CFLAGS) $(EXTRAINC) -o $@ ivyperf.o -L. -livy $(PCRELIB) $(EXTRALIB)

ivyxtprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DXTMAINLOOP -c ivyprobe.c -o $@ $(XTINC)

ivyxtprobe: ivyxtprobe.o ../src/libxtivy.a
	$(CC) -o $@ ivyxtprobe.o -L. $(XTLIB) -lxtivy  $(PCRELIB) $(EXTRALIB)

ivyglibprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DGLIBMAINLOOP -c ivyprobe.c -o ivyglibprobe.o $(GLIBINC)

ivyglibprobe: ivyglibprobe.o ../src/libglibivy.a
	$(CC) -o ivyglibprobe ivyglibprobe.o -L. $(GLIBLIB) -lglibivy $(PCRELIB) $(EXTRALIB)

ivyglutprobe.o : ivyprobe.c
	$(CC)  $(CFLAGS) $(EXTRAINC) $(REGEXP) -DGLUTMAINLOOP -c ivyprobe.c -o ivyglutprobe.o $(GLUTINC)

ivyglutprobe: ivyglutprobe.o ../src/libglutivy.a
	$(CC) -o ivyglutprobe ivyglutprobe.o -L. $(GLUTLIB) -lglutivy $(PCRELIB) $(EXTRALIB

installbins: $(TARGETS)
	test -d $(DESTDIR)$(PREFIX)/bin || mkdirhier $(DESTDIR)$(PREFIX)/bin
	install -m755 ivyprobe $(DESTDIR)$(PREFIX)/bin

installbins_omp:  $(OMP_TARGET)
	test -d $(DESTDIR)$(PREFIX)/bin || mkdirhier $(DESTDIR)$(PREFIX)/bin
	install -m755 ivyprobe_omp $(DESTDIR)$(PREFIX)/bin

install: installbins 
install_omp: installbins_omp


clean:
	rm -fR $(TARGETS) $(OMP_TARGET) *~ *.o
