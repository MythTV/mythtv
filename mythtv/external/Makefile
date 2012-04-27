include ../config.mak

SUBDIRS = FFmpeg zeromq nzmqt qjson

SUBDIRS_ALL = $(addsuffix -all, ${SUBDIRS})
SUBDIRS_INSTALL = $(addsuffix -install, ${SUBDIRS})
SUBDIRS_UNINSTALL = $(addsuffix -uninstall, ${SUBDIRS})
SUBDIRS_CLEAN = $(addsuffix -clean, ${SUBDIRS})
SUBDIRS_DISTCLEAN = $(addsuffix -distclean, ${SUBDIRS})

default:	all
all:		${SUBDIRS_ALL}
install:	${SUBDIRS_INSTALL}
uninstall:	${SUBDIRS_UNINSTALL}
clean:		${SUBDIRS_CLEAN}
distclean:	${SUBDIRS_DISTCLEAN}


FFmpeg-all FFmpeg-install FFmpeg-uninstall FFmpeg-clean FFmpeg-distclean:
	${MAKE} -C FFmpeg ${@:FFmpeg-%=%} 

zeromq-all:
	(cd zeromq ; ./configure --prefix=${PREFIX})
	${MAKE} -C zeromq all

zeromq-install zeromq-uninstall zeromq-clean zeromq-distclean:
	${MAKE} -C zeromq ${@:zeromq-%=%} 

nzmqt-all:
	(cd nzmqt/src ; ${QMAKE})
	${MAKE} -C nzmqt/src all

nzmqt-install nzmqt-uninstall nzmqt-clean nzmqt-distclean:
	${MAKE} -C nzmqt/src ${@:nzmqt-%=%} 

qjson-all:
	(cd qjson ; ${QMAKE})
	${MAKE} -C qjson all

qjson-install qjson-uninstall qjson-clean qjson-distclean:
	${MAKE} -C qjson ${@:qjson-%=%} 

