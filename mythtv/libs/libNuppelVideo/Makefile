CXX=g++

#DEBUG=-O0 -g3 -pg
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS= -Wall -DMMX -D_GNU_SOURCE $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

.SUFFIXES: .cpp .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@

all:   nuvplay nuvrec 

NUVPLAYOBJ = XJ.o minilzo.o RTjpegN.o linearBlend.o NuppelVideoPlayer.o nuvplay.o
NUVPLAYLDF = -L/usr/X11R6/lib -lmp3lame -lX11 -lXext -lXv -lm

NUVRECOBJ = NuppelVideoRecorder.o linearBlend.o minilzo.o RTjpegN.o nuvrec.o
NUVRECLDF = -lmp3lame -lm -lpthread

nuvplay: $(NUVPLAYOBJ)
	@rm -f nuvplay
	$(CXXLINK) $(NUVPLAYOBJ) $(NUVPLAYLDF)

nuvrec: $(NUVRECOBJ)
	@rm -f nuvrec
	$(CXXLINK) $(NUVRECOBJ) $(NUVRECLDF)

install: nuvrec nuvplay 
	 strip nuvrec nuvplay 
	 install -m 755 nuvrec nuvplay /usr/local/bin

clean:	
	rm -f *.o nuvplay nuvrec
