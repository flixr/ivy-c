XTINC = 
CC=gcc
OBJ = timer.o socket.o bus.o 
GOBJ = timer.o socket.o gbus.o
XTOBJ = timer.o xtsocket.o bus.o

all: libbus.a libgbus.a libxtbus.a testbus

gbus.o: bus.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) -o gbus.o bus.c

xtsocket.o: socket.c
	$(CC) -DXTMAINLOOP $(XTINC) -c $(CFLAGS) -o xtsocket.o socket.c

testbus: testbus.o $(OBJ)
	$(CC) -o testbus testbus.o $(OBJ)

libbus.a: $(OBJ)
	ar q libbus.a $(OBJ)

libgbus.a: $(GOBJ)
	ar q libgbus.a $(GOBJ)

libxtbus.a: $(XTOBJ)
	ar q libxtbus.a $(XTOBJ)


