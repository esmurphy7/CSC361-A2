CC = gcc
CFLAGS = -c -Wall -oterm
LIBS = -lpthread -lm

all: rdps rdpr

rdps: rdps.o timer.o packet.o
	$(CC) rdps.o timer.o packet.o -o rdps $(LIBS)
	
rdpr: rdpr.o timer.o packet.o
	$(CC) rdpr.o timer.o packet.o -o rdpr -lm
	
source: rdps.c rdpr.c timer.c packet.c
	$(CC) $(CFLAGS) rdps.c rdpr.c timer.c packet.c
	
clean:
	rm *.o rdps rdpr