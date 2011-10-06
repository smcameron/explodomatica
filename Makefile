
CC=gcc
CFLAGS=-g -W -Wall

GTKCFLAGS = `pkg-config gtk+-2.0 --cflags`
GTKLDFLAGS = `pkg-config gtk+-2.0 --libs`

all:	explodomatica gexplodomatica libexplodomatica.o

ogg_to_pcm.o:	ogg_to_pcm.c ogg_to_pcm.h Makefile
	$(CC) ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} `pkg-config --cflags vorbisfile` \
		-pthread ${WARNFLAG} -c ogg_to_pcm.c

wwviaudio.o:	wwviaudio.c wwviaudio.h ogg_to_pcm.h Makefile
	$(CC) ${WARNFLAG} ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} \
		${DEFINES} \
		-pthread `pkg-config --cflags vorbisfile` \
		-c wwviaudio.c

libexplodomatica.o:	libexplodomatica.c explodomatica.h Makefile
	$(CC) -c libexplodomatica.c

explodomatica:	explodomatica.c explodomatica.h libexplodomatica.o Makefile
	$(CC) ${CFLAGS} -lm -lsndfile -o explodomatica libexplodomatica.o explodomatica.c

gexplodomatica:	gexplodomatica.c libexplodomatica.o explodomatica.h ogg_to_pcm.o wwviaudio.o Makefile
	$(CC) ${CFLAGS} ${GTKCFLAGS} ${GTKLDFLAGS} -lm -lvorbisfile -lportaudio -lsndfile -o gexplodomatica \
			ogg_to_pcm.o wwviaudio.o libexplodomatica.o gexplodomatica.c

clean:
	rm -f explodomatica gexplodomatica *.o


