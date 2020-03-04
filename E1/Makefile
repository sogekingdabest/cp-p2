CFLAGS=-g
OBJS=queue.o main.o
LIBS=-lz
CC=gcc -pthread

all: main

comp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean: 
	rm -f *.o main
