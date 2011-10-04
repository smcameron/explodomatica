
CC=gcc
CFLAGS=-g -W -Wall

GTKCFLAGS = `pkg-config gtk+-2.0 --cflags`
GTKLDFLAGS = `pkg-config gtk+-2.0 --libs`

all:	explodomatica gexplodomatica

explodomatica:	explodomatica.c
	$(CC) ${CFLAGS} -lm -lsndfile -o explodomatica explodomatica.c

gexplodomatica:	gexplodomatica.c
	$(CC) ${CFLAGS} ${GTKCFLAGS} ${GTKLDFLAGS} -o gexplodomatica gexplodomatica.c

clean:
	rm -f explodomatica


