XTINC =-I/usr/X11R6/include
XTLIB =-L/usr/X11R6/lib 
CC=gcc -g -DDEBUG
OBJ = ivyloop.o timer.o ivysocket.o ivy.o 
GOBJ = ivyloop.o timer.o ivysocket.o givy.o
XTOBJ = ivyxtloop.o ivysocket.o ivy.o

all: libivy.a libgivy.a libxtivy.a libivy.so libgivy.so libxtivy.so testivy testxtivy

givy.o: ivy.c
	$(CC) -DGNU_REGEXP -c $(CFLAGS) -o givy.o ivy.c


testivy: testivy.o $(OBJ)
	$(CC) -o testivy testivy.o $(OBJ)
 
testxtivy.o : testivy.c
	$(CC) -c testivy.c -o testxtivy.o $(XTINC) -DXTMAINLOOP

testxtivy: testxtivy.o $(XTOBJ)
	$(CC) -o testxtivy testxtivy.o $(XTOBJ) $(XTLIB) -lXt -lX11

libivy.a: $(OBJ)
	ar q libivy.a $(OBJ)

libgivy.a: $(GOBJ)
	ar q libgivy.a $(GOBJ)

libxtivy.a: $(XTOBJ)
	ar q libxtivy.a $(XTOBJ)

libivy.so: $(OBJ)
	ld -o libivy.so -shared $(OBJ)

libgivy.so: $(GOBJ)
	ld -o libgivy.so -shared $(GOBJ)

libxtivy.so: $(XTOBJ)
	ld -o libxtivy.so -shared $(XTOBJ)
