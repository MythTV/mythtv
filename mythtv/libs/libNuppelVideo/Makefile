CXX=g++

#DEBUG=-O0 -g3 
OPT=-O6 -march=pentiumpro -DMMX -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS= -I/usr/src/linux/include -Wall -D_GNU_SOURCE -D_REENTRANT $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

.SUFFIXES: .cpp .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@

all: libNuppelVideo.a nuppelvideo-config

NUVOBJ  = XJ.o minilzo.o RTjpegN.o effects.o NuppelVideoPlayer.o 
NUVOBJ += NuppelVideoRecorder.o RingBuffer.o ttfont.o osd.o jitterometer.o
NUVOBJ += yuv2rgb.o
NUVLDF = -L/usr/X11R6/lib -lmp3lame -lX11 -lXext -lXv -lttf -lm -lpthread

libNuppelVideo.a: $(NUVOBJ)
	@rm -f libNuppelVideo.a
	ar cru libNuppelVideo.a $(NUVOBJ)
	ranlib libNuppelVideo.a

nuppelvideo-config: 
	echo "#!/bin/sh" > nuppelvideo-config
	echo " " >> nuppelvideo-config
	echo "if [ "'$$1'" -a "'$$1'" = \"--libs\" ]" >> nuppelvideo-config
	echo "then" >> nuppelvideo-config
	echo "  echo \"$(NUVLDF)\"" >> nuppelvideo-config
	echo "fi" >> nuppelvideo-config
	chmod +x nuppelvideo-config
clean:	
	rm -f *.o libNuppelVideo.a nuppelvideo-config
