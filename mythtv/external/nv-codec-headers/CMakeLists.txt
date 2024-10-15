#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#
cmake_minimum_required(VERSION 3.20)

#
# Describe nv-codec-headers
#
project(
  nv-codec-headers
  VERSION 12.2.72.1
  LANGUAGES C)

include(GNUInstallDirs)

#
# Generated files section
#

# Double process the original pc.in file, to handle the double @@.
set(PREFIX PREFIX)
configure_file(ffnvcodec.pc.in ffnvcodec.pc.in @ONLY)
set(PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(${CMAKE_CURRENT_BINARY_DIR}/ffnvcodec.pc.in ffnvcodec.pc @ONLY)

#
# Installation section
#

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ffnvcodec.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
