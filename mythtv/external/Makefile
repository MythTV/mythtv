include ../config.mak

SUBDIRS = FFmpeg libmythdvdnav

ifneq ($(CONFIG_SYSTEM_LIBEXIV2),yes)
    SUBDIRS += libexiv2
endif

ifneq ($(CONFIG_SYSTEM_LIBBLURAY),yes)
    SUBDIRS += libmythbluray
endif

ifneq ($(CONFIG_SYSTEM_LIBUDFREAD),yes)
    SUBDIRS += libudfread
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

libexiv2-all libexiv2-clean libexiv2-distclean libexiv2-install libexiv2-uninstall:
	${MAKE} -C libexiv2 ${@:libexiv2-%=%}

libmythbluray-all libmythbluray-clean libmythbluray-distclean libmythbluray-install libmythbluray-uninstall:
	${MAKE} -C libmythbluray ${@:libmythbluray-%=%}

libmythdvdnav-all libmythdvdnav-clean libmythdvdnav-distclean libmythdvdnav-install libmythdvdnav-uninstall:
	${MAKE} -C libmythdvdnav ${@:libmythdvdnav-%=%}

libudfread-all libudfread-clean libudfread-distclean libudfread-install libudfread-uninstall:
	${MAKE} -C libudfread ${@:libudfread-%=%}

nv-codec-headers-distclean:
	rm -rf nv-codec-headers/install nv-codec-headers/ffnvcodec.pc
