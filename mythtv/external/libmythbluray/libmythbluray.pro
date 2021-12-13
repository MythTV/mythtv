include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythbluray-$$LIBVERSION
CONFIG += thread staticlib warn_off
CONFIG -= qt

QMAKE_CLEAN += $(TARGET)

target.path = $${LIBDIR}

INCLUDEPATH += . ../../
INCLUDEPATH += ./src
INCLUDEPATH += ./src/libbluray
INCLUDEPATH += ./src/libbluray/bdnav

using_system_libudfread: {
    DEFINES += HAVE_LIBUDFREAD
    QMAKE_CFLAGS += $$LIBUDFREAD_CFLAGS
    LIBS         += $$LIBUDFREAD_LIBS
} else {
    INCLUDEPATH += ../libudfread
    DEPENDPATH  += ../libudfread
    LIBS        += -L../libudfread -lmythudfread-$$LIBVERSION
}

bluray_major = 1
bluray_minor = 2
bluray_micro = 1
bluray_version = $$bluray_major"."$$bluray_minor"."$$bluray_micro

DEFINES += BLURAY_VERSION_MAJOR=$$bluray_major
DEFINES += BLURAY_VERSION_MINOR=$$bluray_minor
DEFINES += BLURAY_VERSION_MICRO=$$bluray_micro
DEFINES += BLURAY_VERSION_STRING=\\\"$$bluray_version\\\"
DEFINES += VERSION=\\\"$$bluray_version\\\"

DEFINES += JDK_HOME=\\\"$$JDK_HOME\\\" JAVA_ARCH=\\\"$$JAVA_ARCH\\\"

# can't use config.h because code only checks if defined
DEFINES += HAVE_CONFIG_H=0
using_libxml2:    DEFINES += HAVE_LIBXML2
using_freetype2:  DEFINES += HAVE_FT2
using_fontconfig: DEFINES += HAVE_FONTCONFIG

equals(HAVE_DIRENT_H,    "yes") : DEFINES += HAVE_DIRENT_H
equals(HAVE_DLFCN_H,     "yes") : DEFINES += HAVE_DLFCN_H
equals(HAVE_MNTENT_H,    "yes") : DEFINES += HAVE_MNTENT_H
equals(HAVE_GETMNTENT_R, "yes") : DEFINES += HAVE_GETMNTENT_R
equals(HAVE_PTHREAD_H,   "yes") : DEFINES += HAVE_PTHREAD_H
equals(HAVE_STRINGS_H,   "yes") : DEFINES += HAVE_STRINGS_H
equals(HAVE_SYS_DL_H,    "yes") : DEFINES += HAVE_SYS_DL_H
equals(HAVE_SYS_TIME_H,  "yes") : DEFINES += HAVE_SYS_TIME_H

equals(HAVE_BDJ_J2ME,    "yes") : DEFINES += HAVE_BDJ_J2ME

win32-msvc* {
    CONFIG += qt # probably not needed?
    INCLUDEPATH += ../../libs/libmythbase # for compat.h, also unnecessary?

    # needed for vcxproj
    QMAKE_CXXFLAGS += /TP "/FI compat.h"

    # needed for nmake
    QMAKE_CFLAGS   += /TP "/FI compat.h"
}

# bdnav
HEADERS += src/libbluray/bluray.h src/libbluray/bluray_internal.h src/libbluray/bluray-version.h
HEADERS += src/libbluray/player_settings.h src/libbluray/register.h
HEADERS += src/libbluray/bdj/bdj.h src/libbluray/bdj/bdjo_data.h src/libbluray/bdj/bdjo_parse.h
HEADERS += src/libbluray/bdj/native/bdjo.h src/libbluray/bdj/native/java_awt_BDFontMetrics.h src/libbluray/bdj/native/java_awt_BDGraphics.h
HEADERS += src/libbluray/bdj/native/org_videolan_Libbluray.h org_videolan_Logger.h register_native.h util.h
HEADERS += src/libbluray/bdnav/bdid_parse.h src/libbluray/bdnav/bdmv_parse.h src/libbluray/bdnav/bdparse.h
HEADERS += src/libbluray/bdnav/clpi_data.h src/libbluray/bdnav/clpi_parse.h src/libbluray/bdnav/extdata_parse.h
HEADERS += src/libbluray/bdnav/index_parse.h src/libbluray/bdnav/meta_data.h src/libbluray/bdnav/meta_parse.h
HEADERS += src/libbluray/bdnav/mpls_data.h src/libbluray/bdnav/mpls_parse.h src/libbluray/bdnav/navigation.h
HEADERS += src/libbluray/bdnav/sound_parse.h src/libbluray/bdnav/uo_mask.h src/libbluray/bdnav/uo_mask_table.h
HEADERS += src/libbluray/decoders/graphics_controller.h src/libbluray/decoders/graphics_processor.h
HEADERS += src/libbluray/decoders/hdmv_pids.h
HEADERS += src/libbluray/decoders/ig.h src/libbluray/decoders/ig_decode.h src/libbluray/decoders/m2ts_demux.h
HEADERS += src/libbluray/decoders/m2ts_filter.h
HEADERS += src/libbluray/decoders/overlay.h src/libbluray/decoders/pes_buffer.h src/libbluray/decoders/pg.h
HEADERS += src/libbluray/decoders/pg_decode.h src/libbluray/decoders/rle.h src/libbluray/decoders/textst.h
HEADERS += src/libbluray/decoders/textst_decode.h src/libbluray/decoders/textst_render.h
HEADERS += src/libbluray/disc/aacs.h src/libbluray/disc/bdplus.h src/libbluray/disc/dec.h src/libbluray/disc/disc.h
HEADERS += src/libbluray/disc/enc_info.h src/libbluray/disc/properties.h src/libbluray/disc/udf_fs.h
HEADERS += src/libbluray/hdmv/hdmv_insn.h src/libbluray/hdmv/hdmv_vm.h src/libbluray/hdmv/mobj_parse.h
HEADERS += src/file/dirs.h src/file/dl.h src/file/file.h src/file/filesystem.h src/file/mount.h
HEADERS += src/util/array.h src/util/attributes.h src/util/bits.h src/util/event_queue.h src/util/logging.h
HEADERS += src/util/log_control.h src/util/macro.h src/util/mutex.h src/util/refcnt.h src/util/strutl.h src/util/time.h

SOURCES += src/libbluray/bluray.c src/libbluray/register.c
SOURCES += src/libbluray/bdj/bdj.c src/libbluray/bdj/bdjo_parse.c src/libbluray/bdj/native/bdjo.c
SOURCES += src/libbluray/bdj/native/java_awt_BDFontMetrics.c src/libbluray/bdj/native/java_awt_BDGraphics.c
SOURCES += src/libbluray/bdj/native/org_videolan_Libbluray.c src/libbluray/bdj/native/org_videolan_Logger.c
SOURCES += src/libbluray/bdj/native/register_native.c src/libbluray/bdj/native/util.c
SOURCES += src/libbluray/bdnav/bdid_parse.c src/libbluray/bdnav/bdmv_parse.c src/libbluray/bdnav/clpi_parse.c
SOURCES += src/libbluray/bdnav/extdata_parse.c src/libbluray/bdnav/index_parse.c src/libbluray/bdnav/meta_parse.c
SOURCES += src/libbluray/bdnav/mpls_parse.c src/libbluray/bdnav/navigation.c src/libbluray/bdnav/sound_parse.c
SOURCES += src/libbluray/bdnav/uo_mask.c
SOURCES += src/libbluray/decoders/graphics_controller.c src/libbluray/decoders/graphics_processor.c
SOURCES += src/libbluray/decoders/ig_decode.c src/libbluray/decoders/m2ts_filter.c src/libbluray/decoders/m2ts_demux.c
SOURCES += src/libbluray/decoders/pes_buffer.c src/libbluray/decoders/pg_decode.c
SOURCES += src/libbluray/decoders/rle.c src/libbluray/decoders/textst_decode.c src/libbluray/decoders/textst_render.c
SOURCES += src/libbluray/disc/aacs.c src/libbluray/disc/bdplus.c src/libbluray/disc/dec.c src/libbluray/disc/disc.c
SOURCES += src/libbluray/disc/properties.c src/libbluray/disc/udf_fs.c
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
    macx:javaos = darwin
    win32:javaos = win32
    linux:javaos = linux
    freebsd:javaos = freebsd

    bdjava.depends = src/libbluray/bdj/build.xml
    bdjava.versions = -Dversion='$${BDJ_TYPE}-$$bluray_version' -Djava_version_asm=$${JAVA_CODE_VERSION} -Djava_version_bdj=$${JAVA_CODE_VERSION}
    bdjava.commands = $${ANTBIN} -f $$bdjava.depends -Dbuild=\'build\' -Ddist=\'.libs\' -Dsrc_awt=:java-$${BDJ_TYPE}:java-build-support $$bdjava.versions

    bdjava_clean.commands = $${ANTBIN} -f $$bdjava.depends -Dbuild=\'build\' -Ddist=\'.libs\' -Dversion='$${BDJ_TYPE}-$$bluray_version clean'

    bdjmain.target  = src/libbluray/bdj/.libs/libmythbluray-$${BDJ_TYPE}-"$$bluray_version".jar
    bdjmain.depends = bdjava
    bdjawt.target   = src/libbluray/bdj/.libs/libmythbluray-awt-$${BDJ_TYPE}-"$$bluray_version".jar
    bdjawt.depends  = bdjava

    installjar.path = $${PREFIX}/share/mythtv/jars
    installjar.files = $$bdjmain.target $$bdjawt.target
    installjar.CONFIG += no_check_exist directory

    INSTALLS += installjar

    CLEAN_DEPS += bdjava_clean
    QMAKE_CLEAN += $$bdjmain.target $$bdjawt.target
    PRE_TARGETDEPS += $$bdjmain.target $$bdjawt.target
    QMAKE_EXTRA_TARGETS += bdjava bdjmain bdjawt bdjava_clean
} else {
    macx:javaos = darwin
    win32:javaos = win32
    linux:javaos = linux
    freebsd:javaos = linux  # Original libbluray configure.ac sets this to 'freebsd'
                            # but for the fallback case that we're not actually building
                            # the JAR, it doesn't provide a freebsd version of jni_md.h.
                            # Let's try using the linux version and hope for the best.
}

equals(JDK_HOME, "") {
    INCLUDEPATH += ./jni ./jni/$$javaos
    HEADERS += ./jni/jni.h ./jni/$$javaos/jni_md.h
} else {
    INCLUDEPATH += $${JDK_HOME}/include $${JDK_HOME}/include/$$javaos
}

include (../../libs/libs-targetfix.pro)
