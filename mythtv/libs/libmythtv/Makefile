SUBDIRS = libNuppelVideo examples
DO_MAKE = @ for i in $(SUBDIRS); do make -C $$i $@; done

CXX=g++

#DEBUG=-O0 -g3
OPT=-O6 -march=pentiumpro -DMMX -fomit-frame-pointer -funroll-loops -fexpensive-optimizations -finline-functions

CXXFLAGS  = -IlibNuppelVideo/ -Wall -D_GNU_SOURCE -D_REENTRANT 
CXXFLAGS += -I/usr/include/mysql $(OPT) $(DEBUG)

CXXCOMPILE = $(CXX) $(CXXFLAGS)
CXXLINK = $(CXX) $(CXXFLAGS) -o $@

NUVLIBS = `libNuppelVideo/nuppelvideo-config --libs` `mysql_config --libs`

.SUFFIXES: .cpp .c .o

.cpp.o:
	$(CXXCOMPILE) -c $< -o $@
.c.o:
	$(CXXCOMPILE) -c $< -o $@

all: libNuppelVideo.a examples libmythtv.a mythtv

LIBTVOBJ = tv.o settings.o channel.o frequencies.o
TVOBJ = main.o
TVLDF = -L. -lmythtv -LlibNuppelVideo/ -lNuppelVideo $(NUVLIBS)

libNuppelVideo.a:
	$(MAKE) -C libNuppelVideo

examples:
	$(MAKE) -C examples

libmythtv.a: $(LIBTVOBJ)
	@rm -f libmythtv.a
	ar cru libmythtv.a $(LIBTVOBJ)
	ranlib libmythtv.a

mythtv: $(TVOBJ) libNuppelVideo.a libmythtv.a
	@rm -f mythtv
	$(CXXLINK) $(TVOBJ) $(TVLDF)

#install: tv
#	 strip mythtv
#	 install -m 755 tv /usr/local/bin

clean:	
	rm -f *.o mythtv libmythtv.a
	$(DO_MAKE)
