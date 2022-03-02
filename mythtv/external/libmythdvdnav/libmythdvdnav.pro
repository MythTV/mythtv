include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythdvdnav-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt
target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./dvdnav ./dvdread
INCLUDEPATH += ../../libs

POSTINC =

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}
contains(INCLUDEPATH, /usr/X11R6/include) {
  POSTINC += /usr/X11R6/include
  INCLUDEPATH -= /usr/X11R6/include
}

INCLUDEPATH += $$POSTINC

# for -ldl
LIBS += $$EXTRA_LIBS

# The version numbers here should match the ones defined in libdvdread's 'configure.ac'
dvdread_major = 6
dvdread_minor = 0
dvdread_micro = 0
dvdread_version = $$dvdread_major"."$$dvdread_minor"."$$dvdread_micro

DEFINES += DVDREAD_VERSION_MAJOR=$$dvdread_major
DEFINES += DVDREAD_VERSION_MINOR=$$dvdread_minor
DEFINES += DVDREAD_VERSION_MICRO=$$dvdread_micro
DEFINES += DVDREAD_VERSION=$$dvdread_version
DEFINES += DVDREAD_VERSION_STRING=\\\"$$dvdread_version\\\"

DEFINES += HAVE_AV_CONFIG_H

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# dvdnav
HEADERS += dvdnav/dvdnav_internal.h dvdnav/read_cache.h \
    dvdnav/vm/vm_serialize.h
HEADERS += dvdnav/dvdnav/dvd_types.h dvdnav/dvdnav/dvdnav.h \
    dvdnav/dvdnav/dvdnav_events.h
HEADERS += dvdnav/vm/decoder.h dvdnav/vm/vm.h dvdnav/vm/vmcmd.h \
    dvdnav/vm/getset.h dvdnav/vm/play.h

SOURCES += dvdnav/dvdnav.c dvdnav/read_cache.c dvdnav/navigation.c \
    dvdnav/vm/vm_serialize.c dvdnav/vm/getset.c dvdnav/vm/play.c \
    dvdnav/vm/vmget.c
SOURCES += dvdnav/highlight.c dvdnav/searching.c dvdnav/settings.c
SOURCES += dvdnav/vm/decoder.c dvdnav/vm/vm.c dvdnav/vm/vmcmd.c

# dvdread
HEADERS += dvdread/bswap.h dvdread/dvd_input.h dvdread/dvdread_internal.h
HEADERS += dvdread/dvd_udf.h dvdread/md5.h
HEADERS += dvdread/bitreader.h dvdread/mythdvdreadexp.h
HEADERS += dvdread/dvdread/bitreader.h dvdread/dvdread/dvd_reader.h
HEADERS += dvdread/dvdread/dvd_udf.h dvdread/dvdread/ifo_print.h
HEADERS += dvdread/dvdread/ifo_read.h dvdread/dvdread/ifo_types.h
HEADERS += dvdread/dvdread/nav_print.h dvdread/dvdread/nav_read.h
HEADERS += dvdread/dvdread/nav_types.h

SOURCES += dvdread/dvd_reader.c dvdread/nav_read.c dvdread/ifo_read.c
SOURCES += dvdread/dvd_input.c dvdread/dvd_udf.c dvdread/md5.c
SOURCES += dvdread/nav_print.c dvdread/ifo_print.c dvdread/bitreader.c

inc_dvdnav.path = $${PREFIX}/include/mythtv/dvdnav
inc_dvdnav.files = dvdnav/dvdnav/dvdnav.h dvdnav/dvdnav/dvdnav_events.h dvdnav/dvdnav/dvd_types.h
inc_dvdread.path = $${PREFIX}/include/mythtv/dvdread
inc_dvdread.files = dvdread/dvdread/dvd_reader.h dvdread/dvdread/nav_types.h dvdread/dvdread/ifo_types.h
inc_dvdread.files += dvdread/dvdread/nav_read.h dvdread/dvdread/ifo_read.h

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
