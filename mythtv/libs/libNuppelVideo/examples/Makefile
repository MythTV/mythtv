CXX=g++

#DEBUG=-O0 -g3
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS  = -I../libNuppelVideo/ -Wall -DMMX -D_GNU_SOURCE -D_REENTRANT 
CXXFLAGS += $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

NUVLIBS = `../libNuppelVideo/nuppelvideo-config --libs`

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@
.c.o:
	$(CXXCOMPILE) -c $< -o $@

all: nuvplay nuvrec

NUVPLAYOBJ = nuvplay.o 
NUVPLAYLDF =-L../libNuppelVideo/ -lNuppelVideo $(NUVLIBS)

NUVRECOBJ = nuvrec.o
NUVRECLDF = -L../libNuppelVideo/ -lNuppelVideo $(NUVLIBS)

libNuppelVideo.a:
	$(MAKE) -C ../libNuppelVideo

nuvplay: $(NUVPLAYOBJ) libNuppelVideo.a
	@rm -f nuvplay
	$(CXXLINK) $(NUVPLAYOBJ) $(NUVPLAYLDF)

nuvrec: $(NUVRECOBJ) libNuppelVideo.a
	@rm -f nuvrec
	$(CXXLINK) $(NUVRECOBJ) $(NUVRECLDF)

clean:	
	rm -f *.o nuvplay nuvrec
