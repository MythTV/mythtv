SUBDIRS = libNuppelVideo examples
DO_MAKE = @ for i in $(SUBDIRS); do make -C $$i $@; done

CXX=g++

#DEBUG=-O0 -g3
OPT=-O6 -march=pentiumpro -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS  = -IlibNuppelVideo/ -Wall -DMMX -D_GNU_SOURCE -D_REENTRANT 
CXXFLAGS += $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

NUVLIBS = `libNuppelVideo/nuppelvideo-config --libs`

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@
.c.o:
	$(CXXCOMPILE) -c $< -o $@

all: libNuppelVideo.a examples tv 

TVOBJ = tv.o settings.o channel.o frequencies.o
TVLDF = -LlibNuppelVideo/ -lNuppelVideo $(NUVLIBS)

libNuppelVideo.a:
	$(MAKE) -C libNuppelVideo

examples:
	$(MAKE) -C examples

tv: $(TVOBJ) libNuppelVideo.a
	@rm -f tv
	$(CXXLINK) $(TVOBJ) $(TVLDF)

install: tv
	 strip tv
	 install -m 755 tv /usr/local/bin

clean:	
	rm -f *.o tv
	$(DO_MAKE)
