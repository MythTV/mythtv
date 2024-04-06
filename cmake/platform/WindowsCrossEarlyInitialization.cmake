#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# Perform windows specific customization.
#
if(NOT WIN32 AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "mingw")
  return()
endif()
set(WIN32 ON)

if(ANDROID)
  message(FATAL_ERROR "ANDROID set in Windows Initialization file.")
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${PROJECT_SOURCE_DIR}/cmake/mingw-w64-x86_64.cmake")
endif()
list(APPEND CMDLINE_ARGS
     "-DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}")
