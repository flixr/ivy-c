XTINC =-I/usr/local/X11R6.3/include 
CC=gcc
OBJ = timer.o bussocket.o bus.o 
GOBJ = timer.o bussocket.o gbus.o
XTOBJ = timer.o xtsocket.o bus.o

all: libbus.a libgbus.a libxtbus.a testbus

gbus.o: bus.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) -o gbus.o bus.c

xtsocket.o: bussocket.c
	$(CC) -DXTMAINLOOP $(XTINC) -c $(CFLAGS) -o xtsocket.o bussocket.c

testbus: testbus.o $(OBJ)
	$(CC) -o testbus testbus.o $(OBJ) -lsocket -lnsl

libbus.a: $(OBJ)
	ar q libbus.a $(OBJ)

libgbus.a: $(GOBJ)
	ar q libgbus.a $(GOBJ)

libxtbus.a: $(XTOBJ)
	ar q libxtbus.a $(XTOBJ)


