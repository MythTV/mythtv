SUBDIRS = lib
DO_MAKE = @ for i in $(SUBDIRS); do make -C $$i $@; done

CXX=g++

#DEBUG=-O0 -g3 -pg
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS= -Ilib/ -Wall -DMMX -D_GNU_SOURCE -D_REENTRANT $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

NUVLIBS = `lib/nuppelvideo-config --libs`

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@
.c.o:
	$(CXXCOMPILE) -c $< -o $@

all: libNuppelVideo.a nuvplay nuvrec tv 

NUVPLAYOBJ = nuvplay.o 
NUVPLAYLDF =-Llib/ -lNuppelVideo $(NUVLIBS)

NUVRECOBJ = nuvrec.o
NUVRECLDF = -Llib/ -lNuppelVideo $(NUVLIBS)

TVOBJ = tv.o settings.o channel.o frequencies.o
TVLDF = -Llib/ -lNuppelVideo $(NUVLIBS)

libNuppelVideo.a:
	$(MAKE) -C lib

nuvplay: $(NUVPLAYOBJ) libNuppelVideo.a
	@rm -f nuvplay
	$(CXXLINK) $(NUVPLAYOBJ) $(NUVPLAYLDF)

nuvrec: $(NUVRECOBJ) libNuppelVideo.a
	@rm -f nuvrec
	$(CXXLINK) $(NUVRECOBJ) $(NUVRECLDF)

tv: $(TVOBJ) libNuppelVideo.a
	@rm -f tv
	$(CXXLINK) $(TVOBJ) $(TVLDF)

install: nuvrec nuvplay tv
	 strip nuvrec nuvplay tv
	 install -m 755 nuvrec nuvplay /usr/local/bin

clean:	
	rm -f *.o nuvplay nuvrec tv
	$(DO_MAKE)
