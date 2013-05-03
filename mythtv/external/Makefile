include ../config.mak

SUBDIRS = FFmpeg zeromq nzmqt qjson libmythbluray

ifdef CONFIG_HDHOMERUN
    SUBDIRS += libhdhomerun
endif

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

zeromq-all:	zeromq/Makefile
	${MAKE} -C zeromq all

zeromq-install zeromq-uninstall zeromq-clean zeromq-distclean:
	${MAKE} -C zeromq ${@:zeromq-%=%} DESTDIR=${INSTALL_ROOT}

nzmqt-all:	zeromq-all
	(cd nzmqt/src ; ${QMAKE} -o Makefile)
	${MAKE} -C nzmqt/src all

nzmqt-install nzmqt-uninstall nzmqt-clean nzmqt-distclean:
	${MAKE} -C nzmqt/src ${@:nzmqt-%=%} 

qjson-all:
	(cd qjson ; ${QMAKE} -o Makefile)
	-[ -f qjson/include/QJson ] && \
	  (cd qjson/include ; ${RM} QJson ; ${LN_S} ../src QJson)
	${MAKE} -C qjson all

qjson-install qjson-uninstall qjson-clean qjson-distclean:
	${MAKE} -C qjson ${@:qjson-%=%} 

libhdhomerun-all libhdhomerun-clean libhdhomerun-distclean libhdhomerun-install libhdhomerun-uninstall:
	${MAKE} -C libhdhomerun -f Makefile.lib ${@:libhdhomerun-%=%}
	${MAKE} -C libhdhomerun -f Makefile.app ${@:libhdhomerun-%=%}

libmythbluray-all libmythbluray-clean libmythbluray-distclean libmythbluray-install libmythbluray-uninstall:
	${MAKE} -C libmythbluray ${@:libmythbluray-%=%}

