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

# Fix problem with iconv_xxx vs libiconv_xxx function names.
list(APPEND FF_PLATFORM_ARGS "--extra-cflags=-DLIBICONV_PLUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBICONV_PLUG")

# Add extra X11 libs for linking
set(X11_EXTRA_LIBS X11::Xext X11::Xxf86vm)
