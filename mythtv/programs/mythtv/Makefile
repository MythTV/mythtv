CC=gcc

#DEBUG=-O0 -g3 
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CFLAGS= -Wall -DMMX -D_GNU_SOURCE $(OPT) $(DEBUG)

V4LDIR=/usr/src/linux/drivers/char/
COPTS=$(CFLAGS) -I$(V4LDIR)

all:   nuvplay nuvrec 

strip: nuvrec nuvplay 
	strip nuvrec nuvplay 

RTjpegN.o: RTjpegN.c RTjpegN.h
	$(CC) -c $(COPTS) RTjpegN.c

XJ.o:	XJ.c
	$(CC) -c $(COPTS) XJ.c

minilzo.o: minilzo.c minilzo.h
	$(CC) $(COPTS) -c minilzo.c

linearBlend.o: linearBlend.h linearBlend.c
	$(CC) $(COPTS) -c linearBlend.c

rtjpeg_plugin.o: rtjpeg_plugin.c rtjpeg_plugin.h
	$(CC) $(COPTS) -c rtjpeg_plugin.c

nuvrec:	nuvrec.c RTjpegN.h RTjpegN.o minilzo.o
	$(CC) $(COPTS) -DNUVREC -o nuvrec minilzo.o RTjpegN.o nuvrec.c -lmp3lame -lm

nuvplay: nuvplay.c linearBlend.o RTjpegN.o XJ.o minilzo.o rtjpeg_plugin.o
	$(CC) $(COPTS) -DNUVPLAY -o nuvplay RTjpegN.o minilzo.o XJ.o linearBlend.o \
	rtjpeg_plugin.o nuvplay.c \
	-L/usr/X11R6/lib -lmp3lame -lm -lX11 -lXext -lXv

tv: nuvplay.c nuvrec.c tv.c linearBlend.o RTjpegN.o minilzo.o rtjpeg_plugin.o
	$(CC) $(COPTS) -DTV -o tv RTjpegN.o minilzo.o XJ.o linearBlend.o \
	rtjpeg_plugin.o nuvplay.c nuvrec.c tv.c -L/usr/X11R6/lib -lmp3lame -lm -lX11 -lXext -lXv -lpthread

install: nuvrec nuvplay 
	 strip nuvrec nuvplay 
	 install -m 755 nuvrec nuvplay /usr/local/bin

clean:	
	rm -f *.o nuvplay nuvrec tv
