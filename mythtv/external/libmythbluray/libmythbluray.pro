include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./bdnav
INCLUDEPATH += ../../libs/libmythbase
INCLUDEPATH += ../../libs/libmythtv

win32-msvc* {
    CONFIG += qt
    DEFINES += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H

    # needed for vcxproj
    QMAKE_CXXFLAGS += /TP "/FI compat.h"

    # needed for nmake
    QMAKE_CFLAGS   += /TP "/FI compat.h"

} else {
    DEFINES += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H HAVE_DIRENT_H HAVE_STRINGS_H
}

using_libxml2 {
DEFINES += HAVE_LIBXML2
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

# bdnav
HEADERS += bluray.h register.h 
HEADERS += bdnav/bdid_parse.h bdnav/bdparse.h bdnav/clpi_data.h bdnav/clpi_parse.h bdnav/extdata_parse.h bdnav/index_parse.h
HEADERS += bdnav/meta_data.h bdnav/meta_parse.h bdnav/mpls_parse.h bdnav/navigation.h bdnav/sound_parse.h bdnav/uo_mask_table.h
HEADERS += decoders/graphics_controller.h decoders/graphics_processor.h decoders/ig.h decoders/ig_decode.h decoders/m2ts_demux.h
HEADERS += decoders/overlay.h decoders/pes_buffer.h decoders/pg.h decoders/pg_decode.h
HEADERS += file/dl.h file/file.h file/filesystem.h file/file_mythiowrapper.h
HEADERS += hdmv/hdmv_insn.h hdmv/hdmv_vm.h hdmv/mobj_parse.h util/attributes.h util/bits.h util/logging.h util/log_control.h
HEADERS += util/macro.h util/mutex.h util/strutl.h

SOURCES += bluray.c register.c 
SOURCES += bdnav/bdid_parse.c bdnav/clpi_parse.c bdnav/extdata_parse.c bdnav/index_parse.c bdnav/meta_parse.c bdnav/mpls_parse.c
SOURCES += bdnav/navigation.c bdnav/sound_parse.c
SOURCES += decoders/graphics_controller.c decoders/graphics_processor.c decoders/ig_decode.c decoders/m2ts_demux.c 
SOURCES += decoders/pes_buffer.c decoders/pg_decode.c
SOURCES += file/dir_mythiowrapper.c file/dir_posix.c file/dl_posix.c file/filesystem.c
SOURCES += file/file_mythiowrapper.c file/file_posix.c
SOURCES += hdmv/hdmv_vm.c hdmv/mobj_parse.c hdmv/mobj_print.c
SOURCES += util/bits.c util/logging.c util/strutl.c

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = bluray.h bdnav/*.h hdmv/*.h file/*.h util/*.h

INSTALLS += inc_bdnav

mingw:DEFINES += STDC_HEADERS

using_bdjava {

HEADERS += bdj/bdj.h bdj/bdjo_parser.h bdj/bdj_private.h bdj/bdj_util.h bdj/common.h
SOURCES += bdj/bdj.c bdj/bdjo_parser.c bdj/bdj_util.c 

QMAKE_POST_LINK=/$${ANTBIN} -f bdj/build.xml; /$${ANTBIN} -f bdj/build.xml clean

installjar.path = $${PREFIX}/share/mythtv/jars
installjar.files = libmythbluray.jar

INSTALLS += installjar

QMAKE_CLEAN += libmythbluray.jar
}

mingw {
    dll : contains( TEMPLATE, lib ) {

        # Qt under Linux/UnixMac OS X builds libBlah.a and libBlah.so,
        # but is using the Windows defaults  libBlah.a and    Blah.dll.
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
