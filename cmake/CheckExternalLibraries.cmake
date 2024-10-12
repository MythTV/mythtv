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
include(BuildConfigString)

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# Does the system have threads?
#
include(FindThreads)
set(HAVE_THREADS ${Threads_FOUND}) # avformatdecoder and globalsettings

#
# Check libraries
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
if(ENABLE_VPX)
  pkg_check_modules(LIBVPX "vpx" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBVPX "vpx")
endif()

# mp3lame: fedora:lame-devel debian:libmp3lame-dev
if(ENABLE_MP3LAME AND (NOT MINGW AND NOT MSVC))
  find_package(Lame 3.98.3 MODULE REQUIRED)
  if(Lame_VERSION STREQUAL "unknown")
    message(STATUS "LAME VERSION IS UNKNOWN.")
  endif()
  add_build_config(Lame::Lame "libmp3lame")
  target_compile_definitions(Lame::Lame INTERFACE USING_LIBMP3LAME)
  add_library(lame ALIAS Lame::Lame)
  set(CONFIG_LIBMP3LAME TRUE)
endif()

# vdpau: fedora:libvdpau-devel debian:libvdpau-dev
if(ENABLE_VDPAU)
  # Version 0.2 contains VDP_DECODER_PROFILE_MPEG4_PART2_ASP and
  # vdp_device_create_x11
  pkg_check_modules(VDPAU "vdpau>=0.2" IMPORTED_TARGET)
  add_build_config(PkgConfig::VDPAU "vdpau")
  if(TARGET PkgConfig::VDPAU)
    target_compile_definitions(PkgConfig::VDPAU INTERFACE USING_VDPAU)
  endif()
endif()

# vaapi: fedora:libva-devel debian:libva-dev
if(ENABLE_VAAPI)
  # No need to check for the individual features below.  Libva 1.2 was released
  # in 2013 and contains all these features. Just require libva >= 1.2.
  # ~~~
  # 1.2.0 vaCreateSurfaces(0, 0, 0, 0, 0, 0, 0, 0)
  # 1.1.0 vaGetDisplayDRM
  # 1.1.0 vaGetDisplay
  # 1.0.0 "VA_CHECK_VERSION(1, 0, 0)
  # ~~~
  pkg_check_modules(VAAPI "libva>=1.2" IMPORTED_TARGET)
  add_build_config(PkgConfig::VDPAU "vaapi")
  if(TARGET PkgConfig::VAAPI)
    target_compile_definitions(PkgConfig::VAAPI INTERFACE USING_VAAPI)
    pkg_check_modules(VA-DRM "libva-drm" REQUIRED IMPORTED_TARGET)
    pkg_check_modules(VA-GLX "libva-glx" REQUIRED IMPORTED_TARGET)
    pkg_check_modules(VA-X11 "libva-x11" REQUIRED IMPORTED_TARGET)
    target_link_libraries(
      PkgConfig::VAAPI INTERFACE PkgConfig::VA-X11 PkgConfig::VA-GLX
                                 PkgConfig::VA-DRM)
  endif()
endif()

# ass: fedora:libass-devel debian:libass-dev
if(ENABLE_LIBASS)
  pkg_check_modules(LIBASS "libass>=0.9.10" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBASS "libass")
  if(TARGET PkgConfig::LIBASS)
    target_compile_definitions(PkgConfig::LIBASS INTERFACE USING_LIBASS)
  endif()
endif()

# udev: fedora:libdav1d-devel debian:libdav1d-dev
if(ENABLE_LIBDAV1D)
  pkg_check_modules(LIBDAV1D "dav1d" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBDAV1 "dav1d")
endif()

# aom: fedora:libaom-devel debian:libaom-dev
if(ENABLE_LIBAOM)
  pkg_check_modules(LIBAOM "aom" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBAOM "aom")
endif()

# gnutls: fedora:gnutls-devel debian:libgnutls28-dev
if(ENABLE_GNUTLS)
  pkg_check_modules(GNUTLS "gnutls" IMPORTED_TARGET)
  add_build_config(PkgConfig::GNUTLS "gnutls")
endif()

# ~~~
# udev: fedora:libiec61883-devel debian:libiec61883-dev
# udev: fedora:libavc1394-devel debian:libavc1394-dev
# ~~~
if(ENABLE_FIREWIRE)
  pkg_check_modules(LIBIEC61883 "libiec61883" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBIEC61883 "iec61883")
  pkg_check_modules(LIBAVC1394 "libavc1394" IMPORTED_TARGET)
  add_build_config(PkgConfig::LIBAVC1394 "avc1394")
  if(TARGET PkgConfig::LIBIEC61883 AND TARGET PkgConfig::LIBAVC1394)
    target_link_libraries(PkgConfig::LIBIEC61883
                          INTERFACE PkgConfig::LIBAVC1394)
  endif()
endif()

# sdl2: fedora:SDL2-devel debian:libsdl2-dev
if(ENABLE_SDL2)
  pkg_check_modules(SDL2 "sdl2" IMPORTED_TARGET)
  add_build_config(PkgConfig::SDL2 "sdl2")
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
    target_compile_definitions(PkgConfig::DRM INTERFACE USING_DRM)
    if(TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
      target_compile_definitions(PkgConfig::DRM INTERFACE USING_DRM_VIDEO)
    endif()
    set(HAVE_STRUCT_HDR_METADATA_INFOFRAME TRUE)
  endif()
endif()

#
# See if there is a system libbluray for FFmpeg.
#
# bluray: fedora:libbluray-devel debian:libbluray-dev
pkg_check_modules(SYSTEM_LIBBLURAY "libbluray>=0.9.3" IMPORTED_TARGET)
add_build_config(PkgConfig::SYSTEM_LIBBLURAY "system_libbluray")
if(SYSTEM_LIBBLURAY_FOUND)
  target_compile_definitions(PkgConfig::SYSTEM_LIBBLURAY
                             INTERFACE HAVE_LIBBLURAY)
endif()
