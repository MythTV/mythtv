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
pkg_check_modules(LIBAVCODEC libmythavcodec REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVDEVICE libmythavdevice REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFILTER libmythavfilter REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVFORMAT libmythavformat REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBAVUTIL libmythavutil REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBPOSTPROC libmythpostproc REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWRESAMPLE libmythswresample REQUIRED IMPORTED_TARGET)
pkg_check_modules(LIBSWSCALE libmythswscale REQUIRED IMPORTED_TARGET)
