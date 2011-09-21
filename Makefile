
CC=gcc
CFLAGS=-g -W -Wall

all:	explodomatica

explodomatica:	explodomatica.c
	$(CC) ${CFLAGS} -lm -lsndfile -o explodomatica explodomatica.c

clean:
	rm -f explodomatica


