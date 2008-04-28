PREFIX = /usr/local
BINPATH = ${PREFIX}/bin

all:

install:
	install -m 0755 change-channel-lirc.pl ${BINPATH}
	install -m 0755 change-channel-lirc.sh ${BINPATH}
	install -m 0755 dish-change-channel-lirc.sh ${BINPATH}

uninstall:
	rm -f ${BINPATH}/change-channel-lirc.pl
	rm -f ${BINPATH}/change-channel-lirc.sh
	rm -f ${BINPATH}/dish-change-channel-lirc.sh

