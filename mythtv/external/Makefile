include ../config.mak

SUBDIRS = FFmpeg libmythbluray libmythdvdnav libsamplerate libudfread minilzo
SUBDIRS += libmythsoundtouch
ifeq ($(CONFIG_MYTHLOGSERVER),yes)
    SUBDIRS += zeromq nzmqt
endif

ifdef CONFIG_HDHOMERUN
    SUBDIRS += libhdhomerun
endif

ifdef CONFIG_X11
    SUBDIRS += libXNVCtrl
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
	${MAKE} -C FFmpeg ${@:FFmpeg-%=%} DESTDIR=${INSTALL_ROOT}

zeromq-all:	zeromq/Makefile
	${MAKE} -C zeromq all
ifeq ($(CONFIG_DARWIN),yes)
	install_name_tool -id "@rpath/$(LIBPREF)zmq.1$(SLIBSUF)" zeromq/src/.libs/$(LIBPREF)zmq.1$(SLIBSUF)
endif

zeromq-install zeromq-uninstall zeromq-clean zeromq-distclean:
	${MAKE} -C zeromq ${@:zeromq-%=%} DESTDIR=${INSTALL_ROOT}

nzmqt-all:	zeromq-all
	(cd nzmqt/src ; ${QMAKE} -o Makefile)
	${MAKE} -C nzmqt/src all

nzmqt-install nzmqt-uninstall nzmqt-clean nzmqt-distclean:
	${MAKE} -C nzmqt/src ${@:nzmqt-%=%}

libhdhomerun-all libhdhomerun-clean libhdhomerun-distclean libhdhomerun-install libhdhomerun-uninstall:
	${MAKE} -C libhdhomerun -f Makefile.lib ${@:libhdhomerun-%=%}
	${MAKE} -C libhdhomerun -f Makefile.app ${@:libhdhomerun-%=%}

libmythbluray-all libmythbluray-clean libmythbluray-distclean libmythbluray-install libmythbluray-uninstall:
	${MAKE} -C libmythbluray ${@:libmythbluray-%=%}

libmythdvdnav-all libmythdvdnav-clean libmythdvdnav-distclean libmythdvdnav-install libmythdvdnav-uninstall:
	${MAKE} -C libmythdvdnav ${@:libmythdvdnav-%=%}

libsamplerate-all libsamplerate-clean libsamplerate-distclean libsamplerate-install libsamplerate-uninstall:
	${MAKE} -C libsamplerate ${@:libsamplerate-%=%}

libudfread-all libudfread-clean libudfread-distclean libudfread-install libudfread-uninstall:
	${MAKE} -C libudfread ${@:libudfread-%=%}

minilzo-all minilzo-clean minilzo-distclean minilzo-install minilzo-uninstall:
	${MAKE} -C minilzo ${@:minilzo-%=%}

libmythsoundtouch-all libmythsoundtouch-clean libmythsoundtouch-distclean libmythsoundtouch-install libmythsoundtouch-uninstall:
	${MAKE} -C libmythsoundtouch ${@:libmythsoundtouch-%=%}

libXNVCtrl-all libXNVCtrl-install libXNVCtrl-uninstall libXNVCtrl-clean libXNVCtrl-distclean:
	${MAKE} -C libXNVCtrl -f Makefile.lib ${@:libXNVCtrl-%=%}
