PREFIX = /usr/local
BINPATH = ${PREFIX}/bin

all:

install:
	install -m 0755 changechannel.py ${BINPATH}
	install -m 0755 crcgen.py ${BINPATH}

uninstall:
	rm -f ${BINPATH}/changechannel.py
	rm -f ${BINPATH}/crcgen.py
