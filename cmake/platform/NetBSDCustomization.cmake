#
# Copyright (C) 2025 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "NetBSD")
  return()
endif()

# FFmpeg needs a little help in finding the mp3lame library.
list(APPEND FF_PLATFORM_ARGS "--extra-ldflags=-L/usr/pkg/lib")

# Fix problem with iconv_xxx vs libiconv_xxx function names.
list(APPEND FF_PLATFORM_ARGS "--extra-cflags=-DLIBICONV_PLUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBICONV_PLUG")

if(ENABLE_AUDIO_OSS)
  set(MYTHTV_EXTRA_LIBRARIES libossaudio.so)
endif()
