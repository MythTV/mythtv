CFLAGS = -Wall
PREFIX = /usr/local
MANPATH = ${PREFIX}/man/man1
BINPATH = ${PREFIX}/bin

all: channel.1 channel

version.h: VERSION
	echo "/* This file was automatically generated. */" >version.h
	echo "#define VERSION \"`cat VERSION` (`date +%Y-%m-%d`)\"" >>version.h

channel.o: crc.h serial.h packet.h debug.h remote.h opt.h version.h

crc.o: crc.h 

opt.o: opt.h

packet.o: crc.h serial.h packet.h debug.h

serial.o: crc.h serial.h debug.h

channel: channel.o crc.o opt.o packet.o serial.o

channel.1: channel.c version.h
	$(MAKE) channel
	if ! help2man -N --output=channel.1 ./channel ; then \
		echo "No manual page available." > channel.1 ; fi

install: channel.1 channel
	install -m 0755 channel ${BINPATH}
	install -m 0644 channel.1 ${MANPATH}

uninstall:
	rm -f ${BINPATH}/channel ${MANPATH}/channel.1

PACKAGE=dct-channel-`cat VERSION`
dist: channel.1 version.h
	mkdir -p ${PACKAGE}
	cp [A-Z]* *.[ch1] ${PACKAGE}
	tar cvzf ${PACKAGE}.tar.gz ${PACKAGE}
	rm -r ${PACKAGE}

clean distclean:
	rm -f *.o channel core

veryclean: clean
	rm -f channel.1