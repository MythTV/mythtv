#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# This code includes mysql/mysql.h, with the subdirectory name.  Find
# the parent directory so that this include will work properly.
#
find_file(MYSQL_H mysql/mysql.h)
if(NOT MYSQL_H)
  message(STATUS "Disabling mythzmserver. Missing mysql header file.")
  return()
endif()
cmake_path(GET MYSQL_H PARENT_PATH MYSQL_INC_DIR)
cmake_path(GET MYSQL_INC_DIR PARENT_PATH MYSQL_INC_DIR)

#
# Can we find mysql library?
#
find_program(MYSQL_CONFIG_PROG mysql_config REQUIRED)
if(APPLE)
  execute_process(
    COMMAND ${MYSQL_CONFIG_PROG} --libs
    COMMAND sed -E "s/-arch +[a-z0-9_]+ ?//g"
    RESULT_VARIABLE _exit
    OUTPUT_VARIABLE MYSQL_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
  execute_process(
    COMMAND ${MYSQL_CONFIG_PROG} --libs
    RESULT_VARIABLE _exit
    OUTPUT_VARIABLE MYSQL_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if((NOT ${_exit} EQUAL 0) OR ("${MYSQL_LIBS}" STREQUAL ""))
  message(STATUS "Disabling MythMusiczmserver. Can't find mysql libs.")
  return()
else()
  message(STATUS "Found mysql libs: ${MYSQL_LIBS}")
endif()

#
# Declare the application
#
add_executable(mythzmserver mythzmserver.cpp zmserver.cpp zmserver.h)

#
# All remaining target information
#

target_include_directories(mythzmserver
                           PRIVATE . ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv ${MYSQL_INC_DIR})

target_link_libraries(
  mythzmserver
  PRIVATE PkgConfig::LIBAVCODEC
          PkgConfig::LIBAVFORMAT
          PkgConfig::LIBAVUTIL
          PkgConfig::LIBSWRESAMPLE
          PkgConfig::LIBSWSCALE
          ${MYSQL_LIBS}
          ZLIB::ZLIB)

#
# Installation section
#

install(TARGETS mythzmserver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
