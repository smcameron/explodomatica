
CC=gcc
CFLAGS=-g -W -Wall -pthread

GTKCFLAGS = `pkg-config gtk+-2.0 --cflags`
GTKLDFLAGS = `pkg-config gtk+-2.0 --libs`

all:	explodomatica gexplodomatica libexplodomatica.o

ogg_to_pcm.o:	ogg_to_pcm.c ogg_to_pcm.h Makefile
	$(CC) ${CFLAGS} ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} -pthread `pkg-config --cflags vorbisfile` \
		-pthread ${WARNFLAG} -c ogg_to_pcm.c

wwviaudio.o:	wwviaudio.c wwviaudio.h ogg_to_pcm.h Makefile
	$(CC) ${CFLAGS} ${WARNFLAG} ${DEBUG} ${PROFILE_FLAG} ${OPTIMIZE_FLAG} \
		${DEFINES} \
		-pthread `pkg-config --cflags vorbisfile` \
		-c wwviaudio.c

libexplodomatica.o:	libexplodomatica.c explodomatica.h Makefile
	$(CC) ${CFLAGS} -c libexplodomatica.c

explodomatica:	explodomatica.c explodomatica.h libexplodomatica.o Makefile
	$(CC) ${CFLAGS} -lm -lsndfile -o explodomatica libexplodomatica.o explodomatica.c -lsndfile

gexplodomatica:	gexplodomatica.c libexplodomatica.o explodomatica.h ogg_to_pcm.o wwviaudio.o Makefile
	$(CC) ${CFLAGS} ${GTKCFLAGS} ${GTKLDFLAGS} -pthread -lm -lvorbisfile -lportaudio -lsndfile -o gexplodomatica \
			ogg_to_pcm.o wwviaudio.o libexplodomatica.o gexplodomatica.c -lsndfile ${GTKLDFLAGS} -lvorbisfile -lportaudio -lm

clean:
	rm -f explodomatica gexplodomatica *.o

scan-build:
	make clean
	rm -fr /tmp/explode-scan-build-output
	scan-build -o /tmp/explode-scan-build-output make CC=clang
	xdg-open /tmp/explode-scan-build-output/*/index.html

