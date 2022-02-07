#
# Copyright (C) 2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
  return()
endif()

# FFmpeg needs a little help in finding the mp3lame library.
list(APPEND FF_PLATFORM_ARGS "--extra-ldflags=-L/usr/local/lib")
