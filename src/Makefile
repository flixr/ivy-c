XTINC =-I/usr/X11R6/include
XTLIB =-L/usr/X11R6/lib 
CC=gcc -g -DDEBUG
OBJ = busloop.o timer.o bussocket.o bus.o 
GOBJ = busloop.o timer.o bussocket.o gbus.o
XTOBJ = busxtloop.o bussocket.o bus.o

all: libbus.a libgbus.a libxtbus.a testbus testxtbus

gbus.o: bus.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) -o gbus.o bus.c


testbus: testbus.o $(OBJ)
	$(CC) -o testbus testbus.o $(OBJ)
 
testxtbus.o : testbus.c
	$(CC) -c testbus.c -o testxtbus.o $(XTINC) -DXTMAINLOOP

testxtbus: testxtbus.o $(XTOBJ)
	$(CC) -o testxtbus testxtbus.o $(XTOBJ) $(XTLIB) -lXt -lX11

libbus.a: $(OBJ)
	ar q libbus.a $(OBJ)

libgbus.a: $(GOBJ)
	ar q libgbus.a $(GOBJ)

libxtbus.a: $(XTOBJ)
	ar q libxtbus.a $(XTOBJ)


