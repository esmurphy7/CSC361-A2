CC = gcc
CFLAGS = -c -Wall

all: rdps

rdps: rdps.o
	$(CC) rdps.o -o rdps -lm
	
rdps.o: rdps.c
	$(CC) $(CFLAGS) rdps.c
	
clean:
	rm *.o rdps