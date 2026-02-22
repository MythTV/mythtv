#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Debugging
#
# ~~~
# include(CMakePrintSystemInformation)
# include(CMakePrintHelpers)
# ~~~
include(CheckIncludeFile)
include(CheckSymbolExists)
include(BuildConfigString)
include(SetIfTargetExists)

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# Does the system have threads?
#
include(FindThreads)

#
# Check required system packages
#
include(MythFindQt)
find_package(Iconv REQUIRED)
find_package(expat NO_MODULE)
find_package(ZLIB REQUIRED)
find_package(SQLite3 3.13)
add_build_config(SQLite::SQLite3 "sqlite3")
set(CONFIG_SQLITE3 ${SQLite3_FOUND})

#
# There is a libzip cmake module, but it expects to find the zipcmp tool which
# (on Fedora) is part of a separate libzip-tools package and may or may not be
# installed.  Use pkg-config which only cares about the presence of the library.
#
# libzip: fedora:libzip-devel debian:libzip-dev
pkg_check_modules(LIBZIP "libzip" REQUIRED IMPORTED_TARGET)

# taglib: fedora:taglib-devel debian:libtag1-dev
pkg_check_modules(TAGLIB "taglib>=1.11.1" REQUIRED IMPORTED_TARGET)
add_build_config(PkgConfig::TAGLIB "taglib")

# samplerate: fedora:libsamplerate-devel debian:libsamplerate0-dev
pkg_check_modules(SAMPLERATE samplerate REQUIRED IMPORTED_TARGET)

# MacPorts doesn't include the SoundTouch cmake module, but it does have a
# pkg-config file, so use pkg-config everywhere for consistency.
#
# soundtouch: fedora:soundtouch-devel debian:libsoundtouch-dev
pkg_check_modules(SoundTouch "soundtouch>=1.8.0" REQUIRED IMPORTED_TARGET)

#
# Check optional libraries
#

# freetype2: fedora:freetype-devel debian:libfreetype-dev
if(ENABLE_FREETYPE)
  # See https://github.com/ImageMagick/freetype/blob/main/docs/VERSIONS.TXT for
  # conversion between freetype release number and the (libtool) number returned
  # by pkg-config.
  pkg_check_modules(FT2 "freetype2>=16.2.10" REQUIRED IMPORTED_TARGET)
  add_build_config(PkgConfig::FT2 "freetype2")
  set(HAVE_FT2 ${FT2_FOUND})
endif()

# fontconfig: fedora:fontconfig-devel debian:libfontconfig-dev
if(ENABLE_FONTCONFIG)
  find_package(Fontconfig REQUIRED) # Only as a module
  add_build_config(Fontconfig::Fontconfig "fontconfig")
  set(HAVE_FONTCONFIG 1)
endif()

# xml2: fedora:libxml2-devel debian:libxml2-dev
if(ENABLE_XML2)
  pkg_check_modules(LibXml2 "libxml-2.0" REQUIRED IMPORTED_TARGET)
  add_build_config(PkgConfig::LibXml2 "libxml2")
  set(HAVE_LIBXML2 1)
endif()

# libcrypto: fedora:openssl-devel debian:libssl-dev
if(ENABLE_LIBCRYPTO)
  pkg_check_modules(LIBCRYPTO "libcrypto" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBCRYPTO "libcrypto")
  set(CONFIG_LIBCRYPTO ${LIBCRYPTO_FOUND})
endif()

# ~~~
# dns_sd: fedora:avahi-compat-libdns_sd-devel
#         debian:libavahi-compat-libdnssd-dev
# ~~~
if(ENABLE_LIBDNS_SD)
  pkg_search_module(LIBDNS_SD
                    libdns_sd avahi-compat-libdns_sd
                    IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBDNS_SD "libdns_sd")
  if(LIBDNS_SD_FOUND)
    set(CONFIG_LIBDNS_SD TRUE)
    set(CONFIG_AIRPLAY ${LIBCRYPTO_FOUND})
  endif()
endif()

# libsystemd: fedora:systemd-devel debian:libsystemd-dev
if(ENABLE_SYSTEMD_NOTIFY OR ENABLE_SYSTEMD_JOURNAL)
  pkg_check_modules(SYSTEMD "libsystemd" IMPORTED_TARGET)
  if(SYSTEMD_FOUND AND ENABLE_SYSTEMD_JOURNAL)
    find_file(SYSTEMD_JOURNALD "systemd/sd-journal.h"
              PATHS ${SYSTEMD_INCLUDE_PATH})
    add_build_config(SYSTEMD_JOURNALD "systemd_journal")
    set(CONFIG_SYSTEMD_JOURNAL ON)
  endif()
  if(SYSTEMD_FOUND AND ENABLE_SYSTEMD_NOTIFY)
    find_file(SYSTEMD_NOTIFY "systemd/sd-daemon.h"
              PATHS ${SYSTEMD_INCLUDE_PATH})
    add_build_config(SYSTEMD_NOTIFY "systemd_notify")
    set(CONFIG_SYSTEMD_NOTIFY ON)
  endif()
endif()

# libcec: fedora:libcec-devel debian:libcec-dev
if(ENABLE_LIBCEC)
  pkg_check_modules(LibCEC "libcec" IMPORTED_TARGET)
  add_build_config(PkgConfig::LibCEC "libcec")
  set(CONFIG_LIBCEC ${LibCEC_FOUND})
endif()

## Video codecs

# x264: fedora:x264-devel debian:libx264-dev
find_package(LibX264 0.118)
if(ENABLE_X264 AND LibX264_VERSION)
  add_build_config(LibX264::LibX264 "x264")
  set(CONFIG_LIBX264 ${LibX264_FOUND})
endif()

# x265: fedora:x265-devel debian:libx265-dev
find_package(LibX265)
if(ENABLE_X265 AND LibX265_VERSION)
  add_build_config(LibX265::LibX265 "x265")
  set(CONFIG_LIBX265 ${LibX265_FOUND})
endif()

# xvid: fedora:xvidcore-devel debian:libxvidcore-dev
find_package(LibXvid)
if(ENABLE_XVID AND LibX2vid_VERSION)
  add_build_config(LibXvid::LibXvid "xvid")
  set(CONFIG_LIBXVID ${LibXvid_FOUND})
endif()

# vpx: fedora:libvpx-devel debian:libvpx-dev
# Not used by MythTV code, only by FFmpeg.
if(ENABLE_VPX)
  pkg_check_modules(LIBVPX "vpx" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBVPX "vpx")
endif()

# dav1d: fedora:libdav1d-devel debian:libdav1d-dev
if(ENABLE_LIBDAV1D)
  pkg_check_modules(LIBDAV1D "dav1d" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBDAV1 "dav1d")
endif()

# aom: fedora:libaom-devel debian:libaom-dev
if(ENABLE_LIBAOM)
  pkg_check_modules(LIBAOM "aom" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBAOM "aom")
endif()

## Audio codecs

# mp3lame: fedora:lame-devel debian:libmp3lame-dev
if(ENABLE_MP3LAME AND (NOT MINGW AND NOT MSVC))
  find_package(Lame 3.98.3 MODULE REQUIRED)
  if(Lame_VERSION STREQUAL "unknown")
    message(STATUS "LAME VERSION IS UNKNOWN.")
  endif()
  add_build_config(Lame::Lame "libmp3lame")
  add_library(lame ALIAS Lame::Lame)
  set(CONFIG_LIBMP3LAME TRUE)
endif()

## GPU APIs

if(QT_OPENGL_ES_2 STREQUAL true OR QT_FEATURE_opengles2 GREATER 0)
  message(STATUS "  OpenGL ES found")
  set(_QT_HAS_GLES ON)
endif()
if(_QT_HAS_GLES)
  pkg_check_modules(GLES2 IMPORTED_TARGET glesv2)
  add_build_config(PkgConfig::GLES2 "opengl-es2")
  set(CONFIG_OPENGL ${GLES2_FOUND})
  pkg_check_modules(EGL IMPORTED_TARGET egl)
  add_build_config(PkgConfig::EGL "egl")
  set(CONFIG_EGL ${EGL_FOUND})
  add_library(any_opengl INTERFACE)
  if(TARGET PkgConfig::GLES2)
    target_link_libraries(
      any_opengl INTERFACE Qt${QT_VERSION_MAJOR}::OpenGL PkgConfig::GLES2
                           $<TARGET_NAME_IF_EXISTS:PkgConfig::EGL>)
  endif()
else(_QT_HAS_GLES)
  find_package(OpenGL OPTIONAL_COMPONENTS EGL)
  add_build_config(OpenGL::GL "opengl")
  add_build_config(OpenGL::EGL "egl")
  set_if_target_exists(CONFIG_OPENGL OpenGL::GL)
  set_if_target_exists(CONFIG_EGL OpenGL::EGL)
  if(TARGET OpenGL::GL)
    add_library(any_opengl INTERFACE)
    target_link_libraries(
      any_opengl INTERFACE Qt${QT_VERSION_MAJOR}::OpenGL OpenGL::GL
                           $<TARGET_NAME_IF_EXISTS:OpenGL::EGL>)
  endif()
  if(NOT TARGET any_opengl)
    pkg_check_modules(OsMesa IMPORTED_TARGET osmesa)
    add_build_config(PkgConfig::OsMesa "mesa")
    set_if_target_exists(CONFIG_OPENGL PkgConfig::OsMesa)
    if(TARGET PkgConfig::OsMesa)
      add_library(any_opengl INTERFACE)
      target_link_libraries(
        any_opengl INTERFACE Qt${QT_VERSION_MAJOR}::OpenGL PkgConfig::OsMesa)
    endif()
  endif()
endif(_QT_HAS_GLES)

if ("Qt${QT_VERSION_MAJOR}" EQUAL 5)
  # Set the QT_FEATURE_vulkan variable that Qt6 sets.
  get_property(
    _guiprops
    TARGET Qt${QT_VERSION_MAJOR}::Gui
    PROPERTY INTERFACE_QT_ENABLED_FEATURES)
  if("vulkan" IN_LIST _guiprops)
    set(QT_FEATURE_vulkan ON)
  else()
    set(QT_FEATURE_vulkan OFF)
  endif()
endif()
if(ENABLE_VULKAN AND NOT QT_FEATURE_vulkan)
  message(STATUS "Vulkan not supported by installed Qt")
  set(ENABLE_VULKAN OFF)
endif()
if(ENABLE_VULKAN)
  find_package(Vulkan)
  add_build_config(Vulkan::Vulkan "vulkan")
  if(TARGET Vulkan::Vulkan)
    set(CONFIG_VULKAN TRUE)
    if(ENABLE_GLSLANG AND NOT CMAKE_CROSSCOMPILING)
      list(APPEND CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR})
      pkg_check_modules(GLSLANG IMPORTED_TARGET glslang)
      add_build_config(PkgConfig::GLSLANG "libglslang")
      if(TARGET PkgConfig::GLSLANG)
        set(CONFIG_LIBGLSLANG TRUE)
        pkg_check_modules(Spirv spirv REQUIRED IMPORTED_TARGET)
        pkg_check_modules(SpirvTools SPIRV-Tools REQUIRED IMPORTED_TARGET)
        target_link_libraries(
          Vulkan::Vulkan INTERFACE PkgConfig::GLSLANG PkgConfig::Spirv
                                   PkgConfig::SpirvTools)
        if(LSB_RELEASE_ID STREQUAL "fedora"
           OR (LSB_RELEASE_ID STREQUAL "ubuntu" AND LSB_RELEASE_VERSION_ID
                                                    VERSION_GREATER_EQUAL 20.04
              ))
          target_link_libraries(Vulkan::Vulkan INTERFACE MachineIndependent
                                                         GenericCodeGen)
        endif()
      endif()
      list(REMOVE_ITEM CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR})
    endif()
  endif()
endif()

if(NOT TARGET Vulkan::Vulkan AND NOT TARGET any_opengl)
  message(FATAL_ERROR "Require one of OpenGL or Vulkan. None found!")
endif()

## Display output support

if(ENABLE_X11
   AND NOT APPLE
   AND NOT CMAKE_CROSSCOMPILING)
  find_package(X11)
  if(TARGET X11::X11)
    if(NOT TARGET X11::Xrandr)
      message(FATAL_ERROR "X11 is missing Xrandr support.")
    endif()
    if(NOT TARGET X11::Xrender)
      message(FATAL_ERROR "X11 is missing Xrender support.")
    endif()
    if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
      if(NOT TARGET X11::Xext)
        message(FATAL_ERROR "X11 is missing Xext support.")
      endif()
      if(NOT TARGET X11::Xxf86vm)
        message(FATAL_ERROR "X11 is missing Xxf86vm support.")
      endif()
    endif()
    set(CONFIG_X11 ON)
    add_build_config(X11_FOUND "x11")
  endif()
endif()

if(ENABLE_WAYLAND AND TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
  pkg_check_modules(WAYLAND_CLIENT wayland-client IMPORTED_TARGET)
  add_build_config(PkgConfig::WAYLAND_CLIENT "waylandextras")
  set(CONFIG_WAYLANDEXTRAS ${WAYLAND_CLIENT_FOUND})
endif()

# drm: fedora:libdrm-devel debian:libdrm-dev
if(ENABLE_DRM)
  # ~~~
  # Drop old checks:
  # 2.4.82: first release with DRM_FORMAT_MOD_LINEAR (2017)
  # 2.4.104: first release with hdr_metadata_infoframe (2021)
  # ~~~
  pkg_check_modules(DRM "libdrm>=2.4.104" IMPORTED_TARGET)
  add_build_config(PkgConfig::DRM "drm")
  if(DRM_FOUND)
    set(CONFIG_DRM TRUE)
    set_if_target_exists(CONFIG_DRM_VIDEO Qt${QT_VERSION_MAJOR}::GuiPrivate)
    set(HAVE_STRUCT_HDR_METADATA_INFOFRAME TRUE)
  endif()
endif()

## Hardware acceleration libraries

# vdpau: fedora:libvdpau-devel debian:libvdpau-dev
if(ENABLE_VDPAU AND TARGET X11::X11)
  # Version 0.2 contains VDP_DECODER_PROFILE_MPEG4_PART2_ASP and
  # vdp_device_create_x11
  pkg_check_modules(VDPAU "vdpau>=0.2" IMPORTED_TARGET)
  add_build_config(PkgConfig::VDPAU "vdpau")
  set(CONFIG_VDPAU ${VDPAU_FOUND})
endif()

# vaapi: fedora:libva-devel debian:libva-dev
if(ENABLE_VAAPI)
  pkg_check_modules(VAAPI "libva>=1.2" IMPORTED_TARGET)
  add_build_config(PkgConfig::VAAPI "vaapi")
  if(TARGET PkgConfig::VAAPI)
    set(CONFIG_VAAPI TRUE)

    if(TARGET PkgConfig::DRM)
      pkg_check_modules(VAAPI-DRM "libva-drm" IMPORTED_TARGET)
      add_build_config(PkgConfig::VAAPI-DRM "vaapi_drm")
      if(TARGET PkgConfig::VAAPI-DRM)
        set(CONFIG_VAAPI_DRM TRUE)
        target_link_libraries(PkgConfig::VAAPI INTERFACE PkgConfig::VAAPI-DRM)
      endif()
    endif()

    if(TARGET X11::X11)
      pkg_check_modules(VAAPI-GLX "libva-glx" IMPORTED_TARGET)
      add_build_config(PkgConfig::VAAPI-GLX "vaapi_glx")
      if(TARGET PkgConfig::VAAPI-GLX)
        set(CONFIG_VAAPI_GLX TRUE)
        target_link_libraries(PkgConfig::VAAPI INTERFACE PkgConfig::VAAPI-GLX)
      endif()

      pkg_check_modules(VAAPI-X11 "libva-x11" IMPORTED_TARGET)
      add_build_config(PkgConfig::VAAPI-X11 "vaapi_x11")
      if(TARGET PkgConfig::VAAPI-X11)
        set(CONFIG_VAAPI_X11 TRUE)
        target_link_libraries(PkgConfig::VAAPI INTERFACE PkgConfig::VAAPI-X11)
      endif()
    endif()
  endif()
endif()

# v4l2: fedora:kernel-headers debian:linux-libc-dev
if(ENABLE_V4L2)
  # FreeBSD provides V4L2 library, but not videodev2.h header file.
  message(STATUS "Checking for v4l header file")
  find_file(VIDEODEV2_HEADER linux/videodev2.h)
  if(VIDEODEV2_HEADER)
    message(STATUS "  Found ${VIDEODEV2_HEADER}")
    pkg_check_modules(V4L2 "libv4l2" IMPORTED_TARGET)
    add_build_config(PkgConfig::V4L2 "v4l2")
    if(TARGET PkgConfig::V4L2)
      set(CONFIG_V4L2 TRUE)
      if(TARGET PkgConfig::DRM
         AND NOT APPLE
         AND NOT WIN32)
        add_build_config(TRUE "v4l2prime")
        set(CONFIG_V4L2PRIME TRUE)
      endif()
    endif()
  endif()
endif()

# Use MMAL as a proxy for Raspberry Pi support.  Notes: MMAL didn't work well on
# 64bit systems. It has been deprecated in Debian 11 Bullseye in favor of
# V4L2(?).
if(ENABLE_MMAL)
  cmake_push_check_state(RESET)
  # ~~~
  # Raspbian  9: /opt/vc/include
  # Raspbian 11: /usr/include
  # ~~~
  set(CMAKE_REQUIRED_FLAGS -I/opt/vc/include)
  check_include_file(interface/mmal/mmal.h HAVE_MMAL_H)
  set(CMAKE_REQUIRED_LIBRARIES mmal_core mmal_util mmal_vc_client bcm_host)
  check_symbol_exists(mmal_port_connect interface/mmal/mmal.h
                      HAVE_MMAL_PORT_CONNECT)
  cmake_pop_check_state()

  if(HAVE_MMAL_H AND HAVE_MMAL_PORT_CONNECT)
    message(STATUS "Adding mmal support")
    add_library(mythtv_mmal INTERFACE)
    add_build_config(TRUE "mmal")
    set(CONFIG_MMAL TRUE)
    target_include_directories(mythtv_mmal SYSTEM INTERFACE /opt/vc/include)
    target_link_libraries(mythtv_mmal INTERFACE -L/opt/vc/lib mmal vchostif
                                                vcsm vchiq_arm)
  endif()
endif()

if(ENABLE_MEDIACODEC AND ANDROID)
  message(STATUS "Adding mediacodec support")
  add_library(mediacodec INTERFACE)
  set(CONFIG_MEDIACODEC TRUE)
endif()

## Audio outputs

# alsa: fedora:alsa-lib-devel debian:libasound2-dev
if(ENABLE_AUDIO_ALSA)
  # Can't call find_package("ALSA") on OSX. Use pkg_check_modules. 1.0.16
  # contains SND_PCM_NO_AUTO_RESAMPLE
  pkg_check_modules(ALSA "alsa>=1.0.16" IMPORTED_TARGET)
  add_build_config(PkgConfig::ALSA "alsa")
  set(CONFIG_AUDIO_ALSA ${ALSA_FOUND})
endif()

if(ENABLE_AUDIO_OSS)
  find_file(OSS_HEADER NAMES soundcard.h sys/soundcard.h)
  if(OSS_HEADER)
    add_build_config(TRUE "oss")
    add_library(mythtv_oss INTERFACE)
    set(CONFIG_AUDIO_OSS TRUE)
  endif()
endif()

# pulseaudio: fedora:pulseaudio-libs-devel debian:libpulse-dev
if(ENABLE_AUDIO_PULSE)
  # Can't call find_package("PulseAudio") on OSX.
  pkg_check_modules(PULSEAUDIO libpulse IMPORTED_TARGET)
  add_build_config(PkgConfig::PULSEAUDIO "pulse")
  if(PULSEAUDIO_FOUND)
    set(CONFIG_AUDIO_PULSE TRUE)
    set(CONFIG_AUDIO_PULSEOUTPUT ${ENABLE_AUDIO_PULSEOUTPUT})
  endif()
endif()

# jack: fedora:jack-audio-connection-kit-devel debian:n/a
if(ENABLE_AUDIO_JACK)
  pkg_check_modules(JACK "jack" IMPORTED_TARGET)
  add_build_config(PkgConfig::JACK "jack")
  set(CONFIG_AUDIO_JACK ${JACK_FOUND})
endif()

## Checks for FFmpeg

# ass: fedora:libass-devel debian:libass-dev
if(ENABLE_LIBASS)
  pkg_check_modules(LIBASS "libass>=0.9.10" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBASS "libass")
  set(CONFIG_LIBASS ${LIBASS_FOUND})
endif()

# gnutls: fedora:gnutls-devel debian:libgnutls28-dev
if(ENABLE_GNUTLS)
  pkg_check_modules(GNUTLS "gnutls" IMPORTED_TARGET)
  add_build_config(PkgConfig::GNUTLS "gnutls")
endif()

# sdl2: fedora:SDL2-devel debian:libsdl2-dev
if(ENABLE_SDL2)
  pkg_check_modules(SDL2 "sdl2" IMPORTED_TARGET)
  add_build_config(PkgConfig::SDL2 "sdl2")
endif()

#
# See if there is a system libbluray for FFmpeg.
#
# bluray: fedora:libbluray-devel debian:libbluray-dev
pkg_check_modules(SYSTEM_LIBBLURAY "libbluray>=0.9.3" IMPORTED_TARGET)
add_build_config(PkgConfig::SYSTEM_LIBBLURAY "system_libbluray")
if(TARGET PkgConfig::SYSTEM_LIBBLURAY)
  message(STATUS
    "Found libbluray ${SYSTEM_LIBBLURAY_VERSION} ${SYSTEM_LIBBLURAY_LINK_LIBRARIES}")
endif()

# valgrind - needs valgrind-tools-devel
if(ENABLE_VALGRIND)
  pkg_check_modules(VALGRIND "valgrind" IMPORTED_TARGET)
  set(CONFIG_VALGRIND ${VALGRIND_FOUND})
endif()
