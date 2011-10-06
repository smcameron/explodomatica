
CC=gcc
CFLAGS=-g -W -Wall

GTKCFLAGS = `pkg-config gtk+-2.0 --cflags`
GTKLDFLAGS = `pkg-config gtk+-2.0 --libs`

all:	explodomatica gexplodomatica libexplodomatica.o

libexplodomatica.o:	libexplodomatica.c explodomatica.h
	$(CC) -c libexplodomatica.c

explodomatica:	explodomatica.c explodomatica.h libexplodomatica.o
	$(CC) ${CFLAGS} -lm -lsndfile -o explodomatica libexplodomatica.o explodomatica.c

gexplodomatica:	gexplodomatica.c libexplodomatica.o explodomatica.h
	$(CC) ${CFLAGS} ${GTKCFLAGS} ${GTKLDFLAGS} -lm -lsndfile -o gexplodomatica libexplodomatica.o gexplodomatica.c

clean:
	rm -f explodomatica gexplodomatica libexplodomatica.o


