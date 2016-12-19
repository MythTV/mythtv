include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./src
INCLUDEPATH += ./src/libbluray
INCLUDEPATH += ./src/libbluray/bdnav
INCLUDEPATH += ../libudfread
INCLUDEPATH += ../../libs/libmythbase
INCLUDEPATH += ../../libs/libmythtv

DEPENDPATH += ../libudfread

DEFINES += ENABLE_UDF

bluray_major = 0
bluray_minor = 9
bluray_micro = 3
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
HEADERS += src/libbluray/bluray.h src/libbluray/bluray_internal.h src/libbluray/bluray-version.h
HEADERS += src/libbluray/player_settings.h src/libbluray/register.h
HEADERS += src/libbluray/bdnav/bdid_parse.h src/libbluray/bdnav/bdparse.h src/libbluray/bdnav/clpi_data.h
HEADERS += src/libbluray/bdnav/clpi_parse.h src/libbluray/bdnav/extdata_parse.h src/libbluray/bdnav/index_parse.h
HEADERS += src/libbluray/bdnav/meta_data.h src/libbluray/bdnav/meta_parse.h src/libbluray/bdnav/mpls_parse.h
HEADERS += src/libbluray/bdnav/navigation.h src/libbluray/bdnav/sound_parse.h src/libbluray/bdnav/uo_mask_table.h
HEADERS += src/libbluray/decoders/graphics_controller.h src/libbluray/decoders/graphics_processor.h
HEADERS += src/libbluray/decoders/hdmv_pids.h
HEADERS += src/libbluray/decoders/ig.h src/libbluray/decoders/ig_decode.h src/libbluray/decoders/m2ts_demux.h
HEADERS += src/libbluray/decoders/m2ts_filter.h
HEADERS += src/libbluray/decoders/overlay.h src/libbluray/decoders/pes_buffer.h src/libbluray/decoders/pg.h
HEADERS += src/libbluray/decoders/pg_decode.h src/libbluray/decoders/rle.h src/libbluray/decoders/textst.h
HEADERS += src/libbluray/decoders/textst_decode.h src/libbluray/decoders/textst_render.h
HEADERS += src/libbluray/disc/aacs.h src/libbluray/disc/bdplus.h src/libbluray/disc/dec.h src/libbluray/disc/disc.h
HEADERS += src/libbluray/disc/enc_info.h src/libbluray/disc/udf_fs.h
HEADERS += src/libbluray/hdmv/hdmv_insn.h src/libbluray/hdmv/hdmv_vm.h src/libbluray/hdmv/mobj_parse.h
HEADERS += src/file/dirs.h src/file/dl.h src/file/file.h src/file/filesystem.h src/file/mount.h
HEADERS += src/util/array.h src/util/attributes.h src/util/bits.h src/util/event_queue.h src/util/logging.h
HEADERS += src/util/log_control.h src/util/macro.h src/util/mutex.h src/util/refcnt.h src/util/strutl.h src/util/time.h

SOURCES += src/libbluray/bluray.c src/libbluray/register.c
SOURCES += src/libbluray/bdnav/bdid_parse.c src/libbluray/bdnav/clpi_parse.c src/libbluray/bdnav/extdata_parse.c
SOURCES += src/libbluray/bdnav/index_parse.c src/libbluray/bdnav/meta_parse.c src/libbluray/bdnav/mpls_parse.c
SOURCES += src/libbluray/bdnav/navigation.c src/libbluray/bdnav/sound_parse.c
SOURCES += src/libbluray/decoders/graphics_controller.c src/libbluray/decoders/graphics_processor.c
SOURCES += src/libbluray/decoders/ig_decode.c src/libbluray/decoders/m2ts_filter.c src/libbluray/decoders/m2ts_demux.c
SOURCES += src/libbluray/decoders/pes_buffer.c src/libbluray/decoders/pg_decode.c
SOURCES += src/libbluray/decoders/rle.c src/libbluray/decoders/textst_decode.c src/libbluray/decoders/textst_render.c
SOURCES += src/libbluray/disc/aacs.c src/libbluray/disc/bdplus.c src/libbluray/disc/dec.c src/libbluray/disc/disc.c
SOURCES += src/libbluray/disc/udf_fs.c
SOURCES += src/file/file.c src/file/filesystem.c
SOURCES += src/libbluray/hdmv/hdmv_vm.c src/libbluray/hdmv/mobj_parse.c src/libbluray/hdmv/mobj_print.c
SOURCES += src/util/array.c src/util/bits.c src/util/event_queue.c src/util/logging.c src/util/mutex.c
SOURCES += src/util/refcnt.c src/util/strutl.c src/util/time.c

macx {
    SOURCES += src/file/dir_posix.c src/file/dirs_darwin.c src/file/dl_posix.c src/file/file_posix.c src/file/mount_darwin.c
} else:win32 {
    SOURCES += src/file/dir_win32.c src/file/dirs_win32.c src/file/dl_win32.c src/file/file_win32.c src/file/mount.c
} else {
    SOURCES += src/file/dir_posix.c src/file/dirs_xdg.c src/file/dl_posix.c src/file/file_posix.c src/file/mount.c
}

inc_bdnav.path = $${PREFIX}/include/mythtv/bluray
inc_bdnav.files = src/libbluray/bluray.h src/libbluray/bdnav/*.h src/libbluray/hdmv/*.h src/file/*.h src/util/*.h

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

    HEADERS += src/libbluray/bdj/bdj.h src/libbluray/bdj/bdjo_data.h src/libbluray/bdj/bdjo_parse.h
    HEADERS += src/libbluray/bdj/native/bdjo.h src/libbluray/bdj/native/java_awt_BDFontMetrics.h src/libbluray/bdj/native/java_awt_BDGraphics.h
    HEADERS += src/libbluray/bdj/native/org_videolan_Libbluray.h org_videolan_Logger.h register_native.h util.h
    SOURCES += src/libbluray/bdj/bdj.c src/libbluray/bdj/bdjo_parse.c src/libbluray/bdj/native/bdjo.c
    SOURCES += src/libbluray/bdj/native/java_awt_BDFontMetrics.c src/libbluray/bdj/native/java_awt_BDGraphics.c
    SOURCES += src/libbluray/bdj/native/org_videolan_Libbluray.c src/libbluray/bdj/native/org_videolan_Logger.c
    SOURCES += src/libbluray/bdj/native/register_native.c src/libbluray/bdj/native/util.c

    bdjava.target = src/libbluray/bdj/.libs/libmythbluray-$${BDJ_TYPE}-"$$bluray_version".jar
    bdjava.depends = src/libbluray/bdj/build.xml
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
