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

# DEFINES += LOG_DEBUG TRACE

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# dvdnav
HEADERS += dvdnav/dvdnav/dvd_types.h
HEADERS += dvdnav/dvdnav/dvdnav.h
HEADERS += dvdnav/dvdnav/dvdnav_events.h
HEADERS += dvdnav/dvdnav/version.h
HEADERS += dvdnav/vm/decoder.h
HEADERS += dvdnav/vm/getset.h
HEADERS += dvdnav/vm/play.h
HEADERS += dvdnav/vm/vm.h
HEADERS += dvdnav/vm/vm_serialize.h
HEADERS += dvdnav/vm/vmcmd.h
HEADERS += dvdnav/dvdnav_internal.h
HEADERS += dvdnav/read_cache.h

SOURCES += dvdnav/vm/decoder.c
SOURCES += dvdnav/vm/getset.c
SOURCES += dvdnav/vm/play.c
SOURCES += dvdnav/vm/vm.c
SOURCES += dvdnav/vm/vm_serialize.c
SOURCES += dvdnav/vm/vmcmd.c
SOURCES += dvdnav/vm/vmget.c
SOURCES += dvdnav/dvdnav.c
SOURCES += dvdnav/highlight.c
SOURCES += dvdnav/navigation.c
SOURCES += dvdnav/read_cache.c
SOURCES += dvdnav/searching.c
SOURCES += dvdnav/settings.c

# dvdread
HEADERS += dvdread/dvdread/bitreader.h
HEADERS += dvdread/dvdread/dvd_reader.h
HEADERS += dvdread/dvdread/dvd_udf.h
HEADERS += dvdread/dvdread/ifo_print.h
HEADERS += dvdread/dvdread/ifo_read.h
HEADERS += dvdread/dvdread/ifo_types.h
HEADERS += dvdread/dvdread/nav_print.h
HEADERS += dvdread/dvdread/nav_read.h
HEADERS += dvdread/dvdread/nav_types.h
HEADERS += dvdread/dvdread/version.h
HEADERS += dvdread/bitreader.h
HEADERS += dvdread/bswap.h
HEADERS += dvdread/dvd_input.h
HEADERS += dvdread/dvd_udf.h
HEADERS += dvdread/dvdread_internal.h
HEADERS += dvdread/md5.h
HEADERS += dvdread/mythdvdreadexp.h

SOURCES += dvdread/bitreader.c
SOURCES += dvdread/dvd_input.c
SOURCES += dvdread/dvd_reader.c
SOURCES += dvdread/dvd_udf.c
SOURCES += dvdread/ifo_print.c
SOURCES += dvdread/ifo_read.c
SOURCES += dvdread/md5.c
SOURCES += dvdread/nav_print.c
SOURCES += dvdread/nav_read.c

win32 {
    HEADERS += dvdread/win32_dlfcn.h
}

inc_dvdnav.path = $${PREFIX}/include/mythtv/dvdnav
inc_dvdnav.files += dvdnav/dvdnav/dvdnav.h
inc_dvdnav.files += dvdnav/dvdnav/dvdnav_events.h
inc_dvdnav.files += dvdnav/dvdnav/dvd_types.h
inc_dvdnav.files += dvdnav/dvdnav/version.h

inc_dvdread.path = $${PREFIX}/include/mythtv/dvdread
inc_dvdread.files += dvdread/dvdread/bitreader.h
inc_dvdread.files += dvdread/dvdread/dvd_reader.h
inc_dvdread.files += dvdread/dvdread/dvd_udf.h
inc_dvdread.files += dvdread/dvdread/ifo_print.h
inc_dvdread.files += dvdread/dvdread/ifo_read.h
inc_dvdread.files += dvdread/dvdread/ifo_types.h
inc_dvdread.files += dvdread/dvdread/nav_print.h
inc_dvdread.files += dvdread/dvdread/nav_read.h
inc_dvdread.files += dvdread/dvdread/nav_types.h
inc_dvdread.files += dvdread/dvdread/version.h

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
