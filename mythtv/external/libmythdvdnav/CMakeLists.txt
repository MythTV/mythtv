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
# MythFile* functions).  Cmake only enables PIC code by default on shared
# libraries, so need to enable PIC on the library later after its defined.
#
if(ANDROID OR WIN32)
  set(LIBRARY_TYPE STATIC)
else()
  set(LIBRARY_TYPE SHARED)
endif()

add_library(
  mythdvdnav
  ${LIBRARY_TYPE}
  dvdnav/dvdnav.c
  dvdnav/dvdnav/dvd_types.h
  dvdnav/dvdnav/dvdnav.h
  dvdnav/dvdnav/dvdnav_events.h
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
  dvdread/bitreader.c
  dvdread/bswap.h
  dvdread/dvd_input.c
  dvdread/dvd_input.h
  dvdread/dvd_reader.c
  dvdread/dvd_udf.c
  dvdread/dvdread/bitreader.h
  dvdread/dvdread/dvd_reader.h
  dvdread/dvdread/dvd_udf.h
  dvdread/dvdread/ifo_print.h
  dvdread/dvdread/ifo_read.h
  dvdread/dvdread/ifo_types.h
  dvdread/dvdread/nav_print.h
  dvdread/dvdread/nav_read.h
  dvdread/dvdread/nav_types.h
  dvdread/dvdread_internal.h
  dvdread/ifo_print.c
  dvdread/ifo_read.c
  dvdread/md5.c
  dvdread/md5.h
  dvdread/mythdvdreadexp.h
  dvdread/nav_print.c
  dvdread/nav_read.c
  include/libmythbase/compat.h
  include/libmythtv/mythtvexp.h
  include/libmythtv/io/mythiowrapper.h)

# Don't allow the hidden visibility flag for this target.
set_property(TARGET mythdvdnav PROPERTY C_VISIBILITY_PRESET)

check_include_file(dlfcn.h HAVE_DLFCN_H)
check_include_file(dvdcss/dvdcss.h HAVE_DVDCSS_DVDCSS_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_symbol_exists(gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
  set(HAVE_BIGENDIAN 1)
endif()

# Create stripped down config.h with only the symbols that are used in this
# directory.
configure_file(config.h.in config.h @ONLY)
configure_file(mythconfig.h.in mythconfig.h @ONLY)

# Instead of creating a circular dependency by adding an include path into
# external/libmythdvdnav that points over to libs/libmythxxx, just copy in the
# files that are needed.
add_custom_command(
  OUTPUT include/libmythbase include/libmythtv include/libmythtv/io
  COMMAND ${CMAKE_COMMAND} -E make_directory include/libmythbase
          include/libmythtv/io)
add_custom_command(
  OUTPUT include/libmythbase/compat.h
  DEPENDS include/libmythbase
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${MythTV_SOURCE_DIR}/libs/libmythbase/compat.h include/libmythbase)
add_custom_command(
  OUTPUT include/libmythtv/mythtvexp.h
  DEPENDS include/libmythtv
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${MythTV_SOURCE_DIR}/libs/libmythtv/mythtvexp.h include/libmythtv)
add_custom_command(
  OUTPUT include/libmythtv/io/mythiowrapper.h
  DEPENDS include/libmythtv/io
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    ${MythTV_SOURCE_DIR}/libs/libmythtv/io/mythiowrapper.h include/libmythtv/io)

target_link_libraries(mythdvdnav PUBLIC ${CMAKE_DL_LIBS})
if(APPLE)
  target_link_libraries(mythdvdnav PRIVATE ${APPLE_CORE_FOUNDATION})
endif()
if(HAVE_DVDCSS_DVDCSS_H)
  target_link_libraries(mythdvdnav PRIVATE dvdcss)
endif()

#
# The MythTV customization to dvdnav adds references to the MythFile* functions,
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

# These defines don't appear to be used anywhere in the sources, but they were
# set up by the libmythdvdnav.pro file... so here they are.
set_target_properties(
  mythdvdnav PROPERTIES COMPILE_OPTIONS "-Wno-address-of-packed-member"
                        COMPILE_DEFINITIONS "HAVE_CONFIG_H")

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
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include)

install(
  TARGETS mythdvdnav
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
