#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

set(BLURAY_VERSION "1.2.1")
if(BLURAY_VERSION MATCHES "([0-9]+)\.([0-9]+)\.([0-9]+)")
  set(BLURAY_VERSION_MAJOR ${CMAKE_MATCH_1})
  set(BLURAY_VERSION_MINOR ${CMAKE_MATCH_2})
  set(BLURAY_VERSION_PATCH ${CMAKE_MATCH_3})
else()
  message(FATAL_ERROR "Can't parse bluray version string: ${BLURAY_VERSION}")
endif()

if(APPLE)
  set(JAVAOS "darwin")
elseif(WIN32)
  set(JAVAOS "win32")
else()
  set(JAVAOS "linux")
  # Including FreeBSD
  #
  # Original libbluray configure.ac sets this to 'freebsd' but for the fallback
  # case that were not actually building the JAR, it doesnt provide a freebsd
  # version of jni_md.h.  Lets try using the linux version and hope for the
  # best.
endif()

add_library(
  mythbluray SHARED
  src/file/dirs.h
  src/file/dl.h
  src/file/file.c
  src/file/file.h
  src/file/filesystem.c
  src/file/filesystem.h
  src/file/mount.h
  src/libbluray/bdj/bdj.c
  src/libbluray/bdj/bdj.h
  src/libbluray/bdj/bdjo_data.h
  src/libbluray/bdj/bdjo_parse.c
  src/libbluray/bdj/bdjo_parse.h
  src/libbluray/bdj/native/bdjo.c
  src/libbluray/bdj/native/bdjo.h
  src/libbluray/bdj/native/java_awt_BDFontMetrics.c
  src/libbluray/bdj/native/java_awt_BDFontMetrics.h
  src/libbluray/bdj/native/java_awt_BDGraphics.c
  src/libbluray/bdj/native/java_awt_BDGraphics.h
  src/libbluray/bdj/native/org_videolan_Libbluray.c
  src/libbluray/bdj/native/org_videolan_Libbluray.h
  src/libbluray/bdj/native/org_videolan_Logger.c
  src/libbluray/bdj/native/org_videolan_Logger.h
  src/libbluray/bdj/native/register_native.c
  src/libbluray/bdj/native/register_native.h
  src/libbluray/bdj/native/util.c
  src/libbluray/bdj/native/util.h
  src/libbluray/bdnav/bdid_parse.c
  src/libbluray/bdnav/bdid_parse.h
  src/libbluray/bdnav/bdmv_parse.c
  src/libbluray/bdnav/bdmv_parse.h
  src/libbluray/bdnav/bdparse.h
  src/libbluray/bdnav/clpi_data.h
  src/libbluray/bdnav/clpi_parse.c
  src/libbluray/bdnav/clpi_parse.h
  src/libbluray/bdnav/extdata_parse.c
  src/libbluray/bdnav/extdata_parse.h
  src/libbluray/bdnav/index_parse.c
  src/libbluray/bdnav/index_parse.h
  src/libbluray/bdnav/meta_data.h
  src/libbluray/bdnav/meta_parse.c
  src/libbluray/bdnav/meta_parse.h
  src/libbluray/bdnav/mpls_data.h
  src/libbluray/bdnav/mpls_parse.c
  src/libbluray/bdnav/mpls_parse.h
  src/libbluray/bdnav/navigation.c
  src/libbluray/bdnav/navigation.h
  src/libbluray/bdnav/sound_parse.c
  src/libbluray/bdnav/sound_parse.h
  src/libbluray/bdnav/uo_mask.c
  src/libbluray/bdnav/uo_mask.h
  src/libbluray/bdnav/uo_mask_table.h
  src/libbluray/bluray-version.h
  src/libbluray/bluray.c
  src/libbluray/bluray.h
  src/libbluray/bluray_internal.h
  src/libbluray/decoders/graphics_controller.c
  src/libbluray/decoders/graphics_controller.h
  src/libbluray/decoders/graphics_processor.c
  src/libbluray/decoders/graphics_processor.h
  src/libbluray/decoders/hdmv_pids.h
  src/libbluray/decoders/ig.h
  src/libbluray/decoders/ig_decode.c
  src/libbluray/decoders/ig_decode.h
  src/libbluray/decoders/m2ts_demux.c
  src/libbluray/decoders/m2ts_demux.h
  src/libbluray/decoders/m2ts_filter.c
  src/libbluray/decoders/m2ts_filter.h
  src/libbluray/decoders/overlay.h
  src/libbluray/decoders/pes_buffer.c
  src/libbluray/decoders/pes_buffer.h
  src/libbluray/decoders/pg.h
  src/libbluray/decoders/pg_decode.c
  src/libbluray/decoders/pg_decode.h
  src/libbluray/decoders/rle.c
  src/libbluray/decoders/rle.h
  src/libbluray/decoders/textst.h
  src/libbluray/decoders/textst_decode.c
  src/libbluray/decoders/textst_decode.h
  src/libbluray/decoders/textst_render.c
  src/libbluray/decoders/textst_render.h
  src/libbluray/disc/aacs.c
  src/libbluray/disc/aacs.h
  src/libbluray/disc/bdplus.c
  src/libbluray/disc/bdplus.h
  src/libbluray/disc/dec.c
  src/libbluray/disc/dec.h
  src/libbluray/disc/disc.c
  src/libbluray/disc/disc.h
  src/libbluray/disc/enc_info.h
  src/libbluray/disc/properties.c
  src/libbluray/disc/properties.h
  src/libbluray/disc/udf_fs.c
  src/libbluray/disc/udf_fs.h
  src/libbluray/hdmv/hdmv_insn.h
  src/libbluray/hdmv/hdmv_vm.c
  src/libbluray/hdmv/hdmv_vm.h
  src/libbluray/hdmv/mobj_parse.c
  src/libbluray/hdmv/mobj_parse.h
  src/libbluray/hdmv/mobj_print.c
  src/libbluray/player_settings.h
  src/libbluray/register.c
  src/libbluray/register.h
  src/util/array.c
  src/util/array.h
  src/util/attributes.h
  src/util/bits.c
  src/util/bits.h
  src/util/event_queue.c
  src/util/event_queue.h
  src/util/log_control.h
  src/util/logging.c
  src/util/logging.h
  src/util/macro.h
  src/util/mutex.c
  src/util/mutex.h
  src/util/refcnt.c
  src/util/refcnt.h
  src/util/strutl.c
  src/util/strutl.h
  src/util/time.c
  src/util/time.h)
if(APPLE)
  target_sources(
    mythbluray
    PRIVATE src/file/dir_posix.c src/file/dirs_darwin.c src/file/dl_posix.c
            src/file/file_posix.c src/file/mount_darwin.c)
elseif(WIN32)
  target_sources(
    mythbluray
    PRIVATE src/file/dir_win32.c src/file/dirs_win32.c src/file/dl_win32.c
            src/file/file_win32.c src/file/mount.c)
else()
  target_sources(
    mythbluray
    PRIVATE src/file/dir_posix.c src/file/dirs_xdg.c src/file/dl_posix.c
            src/file/file_posix.c src/file/mount.c)
endif()

# Don't allow the hidden visibility flag for this target.
set_property(TARGET mythbluray PROPERTY C_VISIBILITY_PRESET)

set_target_properties(
  mythbluray
  PROPERTIES SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}
             VERSION
             ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
             COMPILE_OPTIONS "-Wno-address-of-packed-member"
             COMPILE_DEFINITIONS "HAVE_CONFIG_H")

target_link_libraries(
  mythbluray PRIVATE PkgConfig::LibXml2 Fontconfig::Fontconfig PkgConfig::FT2
                     PkgConfig::LIBUDFREAD)

target_include_directories(
  mythbluray
  PUBLIC src
  PRIVATE src/libbluray src/bdnav ${CMAKE_CURRENT_BINARY_DIR})

if(JAVA_HOME)
  target_include_directories(mythbluray PRIVATE ${JAVA_HOME}/include
                                                ${JAVA_HOME}/include/${JAVAOS})
else()
  target_sources(mythbluray PRIVATE jni/jni.h jni/${JAVAOS}/jni_md.h)
  target_include_directories(mythbluray PRIVATE ./jni ./jni/${JAVAOS})
endif()

#
# Add in the code to build the jar files if requested.
#
add_subdirectory(src/libbluray/bdj)

#
# Installation section
#

install(TARGETS mythbluray LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#
# Configuration section
#

#
# By the time this line is reached, there is either a system version of
# libudfread or the embedded version has already been built and installed.
# Either way, the header files are at "udfread/xxx.h". Once the switch from
# qmake to cmake is finalized, the source code can be cleaned up and this define
# (and the corresponding line in config.h.in) can be removed.
#
set(HAVE_LIBUDFREAD 1)

# Create stripped down config.h with only the symbols that are used in this
# directory.
configure_file(config.h.in config.h @ONLY)
