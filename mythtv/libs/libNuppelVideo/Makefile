CXX=g++

#DEBUG=-O0 -g3 
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS= -Wall -DMMX -D_GNU_SOURCE -D_REENTRANT $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

.SUFFIXES: .cpp .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@

all: libNuppelVideo.a

NUVOBJ  = XJ.o minilzo.o RTjpegN.o linearBlend.o NuppelVideoPlayer.o 
NUVOBJ += NuppelVideoRecorder.o RingBuffer.o
NUVLDF = -L/usr/X11R6/lib -lmp3lame -lX11 -lXext -lXv -lm -lpthread

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
	rm -f *.o libNuppelVideo.a
