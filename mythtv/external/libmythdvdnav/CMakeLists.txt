#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

cmake_minimum_required(VERSION 3.20)
project(
  DVDREAD
  VERSION 7.1.1
  LANGUAGES C)
set(DVDREAD_VERSION_MICRO ${DVDREAD_VERSION_PATCH})

set(DVDNAV_MAJOR 7)
set(DVDNAV_MINOR 0)
set(DVDNAV_SUB 1)

#
# Where to find MythTV provided modules
#
# From the sources:
list(APPEND CMAKE_MODULE_PATH "${SUPER_SOURCE_DIR}/cmake")

include(SetCompilerOptions)
include(CheckCCompilerFlag)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(GNUInstallDirs)
include(FindPkgConfig)

# Remove some of the compilation flags that were added to MythTV by
# the -Wall flag.
unset(CFLAGS)
list(APPEND CFLAGS " -Wno-maybe-uninitialized")
list(APPEND CFLAGS " -Wno-unneeded-internal-declaration") # clang
list(APPEND CFLAGS " -Wno-unused-but-set-variable")
list(APPEND CFLAGS " -Wno-unused-variable")
# Remove some of the compilation flags that were added to MythTV by
# the -Wextra flag.
list(APPEND CFLAGS " -Wno-implicit-fallthrough")
list(APPEND CFLAGS " -Wno-unused-parameter")
# libdvdread uses packed structs
list(APPEND CFLAGS " -Wno-address-of-packed-member")

foreach(_FLAG IN LISTS CFLAGS)
  string(SUBSTRING ${_FLAG} 1 -1 _NAME)
  check_c_compiler_flag("${_FLAG}" HAVE_C_${_NAME})
  if(HAVE_C_${_NAME})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_FLAG}")
  endif()
endforeach()

set(LIBDVDNAV_HEADERS
  dvdnav/dvdnav/dvd_types.h
  dvdnav/dvdnav/dvdnav.h
  dvdnav/dvdnav/dvdnav_events.h
  )

set(LIBDVDREAD_HEADERS
  dvdread/dvdread/attributes.h
  dvdread/dvdread/bitreader.h
  dvdread/dvdread/dvd_filesystem.h
  dvdread/dvdread/dvd_reader.h
  dvdread/dvdread/dvd_udf.h
  dvdread/dvdread/ifo_print.h
  dvdread/dvdread/ifo_read.h
  dvdread/dvdread/ifo_types.h
  dvdread/dvdread/nav_print.h
  dvdread/dvdread/nav_read.h
  dvdread/dvdread/nav_types.h
  )

add_library(
  mythdvdnav
  SHARED
  ${LIBDVDNAV_HEADERS}
  dvdnav/dvdnav.c
  dvdnav/dvdnav_internal.h
  dvdnav/highlight.c
  dvdnav/navigation.c
  dvdnav/read_cache.c
  dvdnav/read_cache.h
  dvdnav/searching.c
  dvdnav/settings.c
  dvdnav/vm/decoder.c
  dvdnav/vm/decoder.h
  dvdnav/vm/getset.c
  dvdnav/vm/getset.h
  dvdnav/logger.c
  dvdnav/logger.h
  dvdnav/vm/play.c
  dvdnav/vm/play.h
  dvdnav/vm/vm.c
  dvdnav/vm/vm.h
  dvdnav/vm/vm_serialize.c
  dvdnav/vm/vm_serialize.h
  dvdnav/vm/vmcmd.c
  dvdnav/vm/vmcmd.h
  dvdnav/vm/vmget.c
  ${LIBDVDREAD_HEADERS}
  dvdread/file/filesystem.h
  dvdread/bitreader.c
  dvdread/bswap.h
  dvdread/dvd_input.c
  dvdread/dvd_input.h
  dvdread/dvd_reader.c
  dvdread/dvd_udf.c
  dvdread/dvdread_internal.h
  dvdread/ifo_print.c
  dvdread/ifo_read.c
  dvdread/logger.c
  dvdread/logger.h
  dvdread/md5.c
  dvdread/md5.h
  dvdread/nav_print.c
  dvdread/nav_read.c
  )

if(WIN32)
  set(CMAKE_DL_LIBS dl)
  target_compile_definitions(mythdvdnav PRIVATE WIN32)
  target_sources(mythdvdnav PRIVATE
    dvdread/file/file_win32.c
    msvc/contrib/win32_cs.h
    )
else() # POSIX
  target_sources(mythdvdnav PRIVATE
    dvdread/file/file_posix.c
    )
endif()

target_compile_definitions(mythdvdnav PRIVATE DVDREAD_API_EXPORT)
target_compile_definitions(mythdvdnav PRIVATE DVDNAV_API_EXPORT)

# libdvdcss
set(_NEEDS_DLFCN ON)
if(NOT ANDROID AND NOT WIN32)
  pkg_check_modules(DVDCSS "libdvdcss>=1.2" IMPORTED_TARGET)
  if(TARGET PkgConfig::DVDCSS)
    target_link_libraries(mythdvdnav PRIVATE PkgConfig::DVDCSS)
    set(HAVE_DVDCSS_DVDCSS_H 1)
    set(_NEEDS_DLFCN OFF)
    check_include_file(dvdcss/dvdcpxm.h HAVE_DVDCSS_DVDCPXM_H)
  endif()
endif()
if(_NEEDS_DLFCN)
  check_include_file(dlfcn.h HAVE_DLFCN_H)
  target_link_libraries(mythdvdnav PUBLIC ${CMAKE_DL_LIBS})
endif()

check_symbol_exists(getmntent_r "mntent.h" HAVE_GETMNTENT_R)
check_symbol_exists(strerror_r "string.h" HAVE_STRERROR_R)
check_symbol_exists(strerror_s "string.h" HAVE_STRERROR_S)

# libdvdnav and libdvdread
check_symbol_exists(gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(WORDS_BIGENDIAN 1)
endif()

# Create stripped down config.h with only the symbols that are used in this
# directory.
configure_file(config.h.in config.h @ONLY)

configure_file(dvdread/dvdread/version.h.in dvdread/dvdread/version.h @ONLY)
configure_file(dvdnav/dvdnav/version.h.in dvdnav/dvdnav/version.h @ONLY)

# libdvdnav
target_compile_definitions(mythdvdnav PRIVATE
  HAVE_CONFIG_H
  )

target_include_directories(
  mythdvdnav
  INTERFACE ${CMAKE_INSTALL_INCLUDEDIR}/mythtv
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/dvdnav ${CMAKE_CURRENT_SOURCE_DIR}/dvdread
  # for libdvdnav version.h
  ${CMAKE_CURRENT_BINARY_DIR}/dvdnav/
  ${CMAKE_CURRENT_BINARY_DIR}/dvdnav/dvdnav
  # for libdvdread version.h
  ${CMAKE_CURRENT_BINARY_DIR}/dvdread
  ${CMAKE_CURRENT_BINARY_DIR}/dvdread/dvdread
  )

install(
  TARGETS mythdvdnav
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
  ${LIBDVDNAV_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/dvdnav/dvdnav/version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/dvdnav
  )

install(FILES
  ${LIBDVDREAD_HEADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/dvdread/dvdread/version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/dvdread
  )

configure_file(cmake/mythdvdnav.pc.in mythdvdnav.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/mythdvdnav.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
