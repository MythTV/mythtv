VERSION = 0.1.4
DISTNAME = replex-$(VERSION)
TARNAME = $(DISTNAME).tar.gz	
INCS   = -I..
#CFLAGS =  -g -Wall -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
#CFLAGS =  -g -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -DOUT_DEBUG
CFLAGS =  -g -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE 
LIBS   = -L. 
MFLAG  = -M
OBJS = element.o pes.o mpg_common.o ts.o ringbuffer.o avi.o multiplex.o

SRC  =  avi.c  element.c mpg_common.c pes.c replex.c ringbuffer.c ts.c multiplex.c
HEADERS = element.h pes.h mpg_common.h ts.h ringbuffer.h avi.h replex.h multiplex.h
EXTRA = COPYING README TODO CHANGES
DESTDIR = /usr/local


.PHONY: depend clean install uninstall


all: libreplex.a replex

clean:
	- rm -f *.o .depend  *~ test *.a .depend replex *.tar.gz 
	- rm -rf $(DISTNAME)

libreplex.a: $(OBJS)
	ar -rcs libreplex.a $(OBJS) 

replex: libreplex.a replex.o
	$(CC) -o replex replex.o -L. -lreplex

dist: $(SRC) $(HEADERS) Makefile
	mkdir $(DISTNAME)
	cp $(SRC) $(HEADERS) $(EXTRA) Makefile $(DISTNAME) 
	tar zcf $(TARNAME) $(DISTNAME) 
	rm -rf $(DISTNAME) 

%.o:    %.c %.h
	$(CC) -c $(CFLAGS) $(INCS) $(DEFINES) $<

install: libreplex.a replex
	install -m 644 libreplex.a $(DESTDIR)/lib/
	install -m 755 replex $(DESTDIR)/bin/

uninstall:
	rm -f $(DESTDIR)/lib/libreplex.a
	rm -f $(DESTDIR)/bin/replex


.depend: 
	$(CC) $(DEFINES) $(MFLAG) $(SRC) $(CSRC) $(CPPSRC) $(INCS)> .depend



include .depend
