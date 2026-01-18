#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# This is set up to be a separate sub-project, but is currently
# included by the MythTV sub-project.
project(
  DVDREAD
  VERSION 6.0.0
  LANGUAGES C)
set(DVDREAD_VERSION_MICRO ${DVDREAD_VERSION_PATCH})

set(DVDNAV_MAJOR 6)
set(DVDNAV_MINOR 0)
set(DVDNAV_SUB 0)

include(CheckIncludeFile)
include(CheckSymbolExists)

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

#
# Compile this library as a static library on Android because of the circular
# linking dependency issue with libmythtv (specifically the calls to the
# MythDVD_* functions).  Cmake only enables PIC code by default on shared
# libraries, so need to enable PIC on the library later after its defined.
#
if(ANDROID OR WIN32)
  set(LIBRARY_TYPE STATIC)
else()
  set(LIBRARY_TYPE SHARED)
endif()

set(LIBDVDNAV_HEADERS
  dvdnav/dvdnav/dvd_types.h
  dvdnav/dvdnav/dvdnav.h
  dvdnav/dvdnav/dvdnav_events.h
  )

set(LIBDVDREAD_HEADERS
  dvdread/dvdread/bitreader.h
  dvdread/dvdread/dvd_reader.h
  dvdread/dvdread/dvd_udf.h
  dvdread/dvdread/ifo_print.h
  dvdread/dvdread/ifo_read.h
  dvdread/dvdread/ifo_types.h
  dvdread/dvdread/mythdvdio.h
  dvdread/dvdread/nav_print.h
  dvdread/dvdread/nav_read.h
  dvdread/dvdread/nav_types.h
  )

add_library(
  mythdvdnav
  ${LIBRARY_TYPE}
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
  dvdread/bitreader.c
  dvdread/bswap.h
  dvdread/dvd_input.c
  dvdread/dvd_input.h
  dvdread/dvd_reader.c
  dvdread/dvd_udf.c
  dvdread/dvdread_internal.h
  dvdread/ifo_print.c
  dvdread/ifo_read.c
  dvdread/md5.c
  dvdread/md5.h
  dvdread/nav_print.c
  dvdread/nav_read.c
  )

if(WIN32)
  target_sources(mythdvdnav PRIVATE
    dvdread/win32_dlfcn.h
    )
endif()

# Don't allow the hidden visibility flag for this target.
set_property(TARGET mythdvdnav PROPERTY C_VISIBILITY_PRESET)

# libdvdread
pkg_check_modules(DVDCSS "libdvdcss>=1.2" IMPORTED_TARGET)
if(TARGET PkgConfig::DVDCSS)
  target_link_libraries(mythdvdnav PRIVATE PkgConfig::DVDCSS)
  set(HAVE_DVDCSS_DVDCSS_H 1)
else()
  check_include_file(dlfcn.h HAVE_DLFCN_H)
  target_link_libraries(mythdvdnav PUBLIC ${CMAKE_DL_LIBS})
endif()

check_include_file(sys/param.h HAVE_SYS_PARAM_H)

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

#
# The MythTV customization to dvdnav adds references to the MythDVD_* functions,
# which are defined in libs/libmythtv. to allow these symbols to remain
# undefined while linking mythdvdnav, the "-Wl,--no-undefined" option cannot be
# a part of the link line. The android toolchain file adds this argument by
# default, so it needs to be stripped here.
#
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  if(CMAKE_SHARED_LINKER_FLAGS)
    string(REPLACE "-Wl,--no-undefined" "" CMAKE_SHARED_LINKER_FLAGS
                   ${CMAKE_SHARED_LINKER_FLAGS})
  endif()

  # Compiles with clang on Apple specifically need "-undefined dynamic_lookup"
  # added to the linker flags.  This prevents the linker from failing
  # immediately with complaints about undefined symbols, and defers the lookup
  # to the loader.  Its not clear if this helps on Android (since the explicit
  # opposite has just been removed), but it couldn't hurt.
  target_link_options(mythdvdnav PRIVATE -undefined dynamic_lookup)
endif()

#
# Compile PIC code on android.  The default for PIC code is ON for shared
# libraries, but on android this library is built as a static library because of
# the circular linking dependency issue with libmythtv.
#
if(ANDROID)
  set_target_properties(
    mythdvdnav PROPERTIES POSITION_INDEPENDENT_CODE ON
                          SUFFIX "-${CMAKE_PROJECT_VERSION_MAJOR}.a")
endif()

target_include_directories(
  mythdvdnav
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dvdnav ${CMAKE_CURRENT_SOURCE_DIR}/dvdread
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include
  # for libdvdnav version.h
  ${CMAKE_CURRENT_BINARY_DIR}/dvdnav/dvdnav
  # for libdvdread version.h
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
