#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# FFmpeg is required. This can only find the MythTV version that was just
# installed, because its looking for libmythavcodec , not libavcodec.
#
# Empty MYTH_FFMPEG_BUILD_SUFFIX -> libmythavcodec (the default).
if(MYTH_FFMPEG_BUILD_SUFFIX STREQUAL "")
  message(STATUS "Looking for MythTV FFmpeg wrappers (unsuffixed pkg-config names)")
else()
  message(STATUS "Looking for MythTV FFmpeg wrappers with suffix '${MYTH_FFMPEG_BUILD_SUFFIX}'")
endif()

pkg_check_modules(LIBAVCODEC libmythavcodec${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVDEVICE libmythavdevice${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFILTER libmythavfilter${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFORMAT libmythavformat${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVUTIL libmythavutil${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWRESAMPLE libmythswresample${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWSCALE libmythswscale${MYTH_FFMPEG_BUILD_SUFFIX} REQUIRED IMPORTED_TARGET)
