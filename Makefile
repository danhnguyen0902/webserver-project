CC = gcc
CFLAGS = -O2 -Wall -I .

# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: server cgi

server: server.c csapp.o
	$(CC) $(CFLAGS) -o sysstatd server.c csapp.o threadpool.h list.h list.c threadpool.c $(LIB)

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

cgi:
	(cd cgi-bin; make)

clean:
	rm -f *.o server *~
	(cd cgi-bin; make clean)

