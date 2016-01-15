include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./libbluray
INCLUDEPATH += ./libbluray/bdnav
INCLUDEPATH += ../libudfread
INCLUDEPATH += ../../libs/libmythbase
INCLUDEPATH += ../../libs/libmythtv

DEPENDPATH += ../libudfread

DEFINES += ENABLE_UDF

bluray_major = 0
bluray_minor = 9
bluray_micro = 2
bluray_version = $$bluray_major"."$$bluray_minor"."$$bluray_micro

DEFINES += BLURAY_VERSION_MAJOR=$$bluray_major
DEFINES += BLURAY_VERSION_MINOR=$$bluray_minor
DEFINES += BLURAY_VERSION_MICRO=$$bluray_micro
DEFINES += BLURAY_VERSION_STRING=\\\"$$bluray_version\\\"

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

QMAKE_CLEAN += $(TARGET)

# bdnav
HEADERS += libbluray/bluray.h libbluray/bluray_internal.h libbluray/bluray-version.h
HEADERS += libbluray/player_settings.h libbluray/register.h
HEADERS += libbluray/bdnav/bdid_parse.h libbluray/bdnav/bdparse.h libbluray/bdnav/clpi_data.h
HEADERS += libbluray/bdnav/clpi_parse.h libbluray/bdnav/extdata_parse.h libbluray/bdnav/index_parse.h
HEADERS += libbluray/bdnav/meta_data.h libbluray/bdnav/meta_parse.h libbluray/bdnav/mpls_parse.h
HEADERS += libbluray/bdnav/navigation.h libbluray/bdnav/sound_parse.h libbluray/bdnav/uo_mask_table.h
HEADERS += libbluray/decoders/graphics_controller.h libbluray/decoders/graphics_processor.h
HEADERS += libbluray/decoders/hdmv_pids.h
HEADERS += libbluray/decoders/ig.h libbluray/decoders/ig_decode.h libbluray/decoders/m2ts_demux.h
HEADERS += libbluray/decoders/m2ts_filter.h
HEADERS += libbluray/decoders/overlay.h libbluray/decoders/pes_buffer.h libbluray/decoders/pg.h
HEADERS += libbluray/decoders/pg_decode.h libbluray/decoders/rle.h libbluray/decoders/textst.h
HEADERS += libbluray/decoders/textst_decode.h libbluray/decoders/textst_render.h
HEADERS += libbluray/disc/aacs.h libbluray/disc/bdplus.h libbluray/disc/dec.h libbluray/disc/disc.h
HEADERS += libbluray/disc/enc_info.h libbluray/disc/udf_fs.h
HEADERS += libbluray/hdmv/hdmv_insn.h libbluray/hdmv/hdmv_vm.h libbluray/hdmv/mobj_parse.h
HEADERS += file/dirs.h file/dl.h file/file.h file/filesystem.h file/mount.h
HEADERS += util/array.h util/attributes.h util/bits.h util/logging.h util/log_control.h
HEADERS += util/macro.h util/mutex.h util/refcnt.h util/strutl.h util/time.h

SOURCES += libbluray/bluray.c libbluray/register.c 
SOURCES += libbluray/bdnav/bdid_parse.c libbluray/bdnav/clpi_parse.c libbluray/bdnav/extdata_parse.c
SOURCES += libbluray/bdnav/index_parse.c libbluray/bdnav/meta_parse.c libbluray/bdnav/mpls_parse.c
SOURCES += libbluray/bdnav/navigation.c libbluray/bdnav/sound_parse.c
SOURCES += libbluray/decoders/graphics_controller.c libbluray/decoders/graphics_processor.c
SOURCES += libbluray/decoders/ig_decode.c libbluray/decoders/m2ts_filter.c libbluray/decoders/m2ts_demux.c 
SOURCES += libbluray/decoders/pes_buffer.c libbluray/decoders/pg_decode.c
SOURCES += libbluray/decoders/rle.c libbluray/decoders/textst_decode.c libbluray/decoders/textst_render.c
SOURCES += libbluray/disc/aacs.c libbluray/disc/bdplus.c libbluray/disc/dec.c libbluray/disc/disc.c
SOURCES += libbluray/disc/udf_fs.c
SOURCES += file/file.c file/filesystem.c
SOURCES += libbluray/hdmv/hdmv_vm.c libbluray/hdmv/mobj_parse.c libbluray/hdmv/mobj_print.c
SOURCES += util/array.c util/bits.c util/logging.c util/mutex.c util/refcnt.c util/strutl.c util/time.c

macx {
    SOURCES += file/dir_posix.c file/dirs_darwin.c file/dl_posix.c file/file_posix.c file/mount_darwin.c
} else:win32 {
    SOURCES += file/dir_win32.c file/dirs_win32.c file/dl_win32.c file/file_win32.c file/mount.c
} else {
    SOURCES += file/dir_posix.c file/dirs_xdg.c file/dl_posix.c file/file_posix.c file/mount.c
}

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = libbluray/bluray.h libbluray/bdnav/*.h libbluray/hdmv/*.h file/*.h util/*.h

INSTALLS += inc_bdnav

mingw:DEFINES += STDC_HEADERS

using_bdjava {
    DEFINES += USING_BDJAVA VERSION=\\\"$$bluray_version\\\"
    using_freetype2:DEFINES += HAVE_FT2
    using_fontconfig:DEFINES += HAVE_FONTCONFIG

    macx:javaos = darwin
    win32:javaos = win32
    linux:javaos = linux
    freebsd:javaos = freebsd

    equals(JDK_HOME, "") {
        INCLUDEPATH += ./jni ./jni/$$javaos
        HEADERS += ./jni/jni.h ./jni/$$javaos/jni_md.h
    } else {
        INCLUDEPATH += $${JDK_HOME}/include $${JDK_HOME}/include/$$javaos
    }

    HEADERS += libbluray/bdj/bdj.h libbluray/bdj/bdjo_data.h libbluray/bdj/bdjo_parse.h
    HEADERS += libbluray/bdj/native/bdjo.h libbluray/bdj/native/java_awt_BDFontMetrics.h libbluray/bdj/native/java_awt_BDGraphics.h
    HEADERS += libbluray/bdj/native/org_videolan_Libbluray.h org_videolan_Logger.h register_native.h util.h
    SOURCES += libbluray/bdj/bdj.c libbluray/bdj/bdjo_parse.c libbluray/bdj/native/bdjo.c
    SOURCES += libbluray/bdj/native/java_awt_BDFontMetrics.c libbluray/bdj/native/java_awt_BDGraphics.c
    SOURCES += libbluray/bdj/native/org_videolan_Libbluray.c libbluray/bdj/native/org_videolan_Logger.c
    SOURCES += libbluray/bdj/native/register_native.c libbluray/bdj/native/util.c

    bdjava.target = libbluray/bdj/.libs/libmythbluray-$${BDJ_TYPE}-"$$bluray_version".jar
    bdjava.depends = libbluray/bdj/build.xml
    bdjava.commands = $${ANTBIN} -f $$bdjava.depends -Dbuild=\'build\' -Ddist=\'.libs\' -Dsrc_awt=:java-$${BDJ_TYPE} -Dversion='$${BDJ_TYPE}-$$bluray_version'

    bdjava_clean.commands = $${ANTBIN} -f $$bdjava.depends -Dbuild=\'build\' -Ddist=\'.libs\' -Dversion='$${BDJ_TYPE}-$$bluray_version clean'

    installjar.path = $${PREFIX}/share/mythtv/jars
    installjar.files = $$bdjava.target

    INSTALLS += installjar

    CLEAN_DEPS += bdjava_clean
    QMAKE_CLEAN += $$bdjava.target
    PRE_TARGETDEPS += $$bdjava.target
    QMAKE_EXTRA_TARGETS += bdjava bdjava_clean
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
