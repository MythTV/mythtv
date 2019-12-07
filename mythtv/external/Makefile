include ../config.mak

SUBDIRS = FFmpeg libmythdvdnav libudfread
SUBDIRS += libmythsoundtouch

ifneq ($(CONFIG_LIBBLURAY_EXTERNAL),yes)
    SUBDIRS += libmythbluray
endif

SUBDIRS_ALL = $(addsuffix -all, ${SUBDIRS})
SUBDIRS_INSTALL = $(addsuffix -install, ${SUBDIRS})
SUBDIRS_UNINSTALL = $(addsuffix -uninstall, ${SUBDIRS})
SUBDIRS_CLEAN = $(addsuffix -clean, ${SUBDIRS})
SUBDIRS_DISTCLEAN = $(addsuffix -distclean, ${SUBDIRS} nv-codec-headers)

default:	all
all:		${SUBDIRS_ALL}
install:	${SUBDIRS_INSTALL}
uninstall:	${SUBDIRS_UNINSTALL}
clean:		${SUBDIRS_CLEAN}
distclean:	${SUBDIRS_DISTCLEAN}


FFmpeg-all FFmpeg-install FFmpeg-uninstall FFmpeg-clean FFmpeg-distclean:
	${MAKE} -C FFmpeg ${@:FFmpeg-%=%} DESTDIR=${INSTALL_ROOT}

libmythbluray-all libmythbluray-clean libmythbluray-distclean libmythbluray-install libmythbluray-uninstall:
	${MAKE} -C libmythbluray ${@:libmythbluray-%=%}

libmythdvdnav-all libmythdvdnav-clean libmythdvdnav-distclean libmythdvdnav-install libmythdvdnav-uninstall:
	${MAKE} -C libmythdvdnav ${@:libmythdvdnav-%=%}

libudfread-all libudfread-clean libudfread-distclean libudfread-install libudfread-uninstall:
	${MAKE} -C libudfread ${@:libudfread-%=%}

libmythsoundtouch-all libmythsoundtouch-clean libmythsoundtouch-distclean libmythsoundtouch-install libmythsoundtouch-uninstall:
	${MAKE} -C libmythsoundtouch ${@:libmythsoundtouch-%=%}

nv-codec-headers-distclean:
	rm -rf nv-codec-headers/install nv-codec-headers/ffnvcodec.pc
