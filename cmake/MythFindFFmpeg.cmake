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
# installed, because its looking for libmythavcodec-${PROJECT_VERSION_MAJOR},
# not libavcodec.
#
pkg_check_modules(LIBAVCODEC libmythavcodec-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVDEVICE libmythavdevice-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFILTER libmythavfilter-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFORMAT libmythavformat-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVUTIL libmythavutil-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWRESAMPLE libmythswresample-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWSCALE libmythswscale-${PROJECT_VERSION_MAJOR} REQUIRED IMPORTED_TARGET)
