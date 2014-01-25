include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythdvdnav-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./dvdnav ./dvdread
INCLUDEPATH += ../../libs/libmythbase
INCLUDEPATH += ../../libs/libmythtv

# for -ldl
LIBS += $$EXTRA_LIBS

DEFINES += HAVE_AV_CONFIG_H

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# dvdnav
HEADERS += dvdnav/dvdnav_internal.h dvdnav/read_cache.h dvdnav/remap.h \
    dvdnav/vm/vm_serialize.h
HEADERS += dvdnav/vm/decoder.h dvdnav/vm/vm.h dvdnav/vm/vmcmd.h

SOURCES += dvdnav/dvdnav.c dvdnav/read_cache.c dvdnav/navigation.c \
    dvdnav/vm/vm_serialize.c
SOURCES += dvdnav/highlight.c dvdnav/searching.c dvdnav/settings.c
SOURCES += dvdnav/remap.c dvdnav/vm/decoder.c dvdnav/vm/vm.c
SOURCES += dvdnav/vm/vmcmd.c

# dvdread
HEADERS += dvdread/bswap.h dvdread/dvd_input.h dvdread/dvdread_internal.h
HEADERS += dvdread//dvd_udf.h dvdread/md5.h
HEADERS += dvdread/bitreader.h

SOURCES += dvdread/dvd_reader.c dvdread/nav_read.c dvdread/ifo_read.c
SOURCES += dvdread/dvd_input.c dvdread/dvd_udf.c dvdread/md5.c
SOURCES += dvdread/nav_print.c dvdread/ifo_print.c dvdread/bitreader.c

inc_dvdnav.path = $${PREFIX}/include/mythtv/dvdnav
inc_dvdnav.files = dvdnav/dvdnav.h dvdnav/dvdnav_events.h dvdnav/dvd_types.h
inc_dvdread.path = $${PREFIX}/include/mythtv/dvdread
inc_dvdread.files = dvdread/dvd_reader.h dvdread/nav_types.h dvdread/ifo_types.h
inc_dvdread.files += dvdread/nav_read.h dvdread/ifo_read.h

INSTALLS += inc_dvdnav inc_dvdread

mingw {
    DEFINES += STDC_HEADERS
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults libBlah.a and Blah.dll.
        #
        # So that our dependency targets work between SUBDIRS, override:
        #
        TARGET = lib$${TARGET}


        # Windows doesn't have a nice variable like LD_LIBRARY_PATH,
        # which means make install would be broken without extra steps.
        # As a workaround, we store dlls with exes. Also improves debugging!
        #
        target.path = $${PREFIX}/bin
    }
}
