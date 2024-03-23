#
# Copyright (C) 2024 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(
  mythtranscode
  audioreencodebuffer.cpp
  audioreencodebuffer.h
  cutter.cpp
  cutter.h
  external/replex/element.cpp
  external/replex/element.h
  external/replex/mpg_common.cpp
  external/replex/mpg_common.h
  external/replex/multiplex.cpp
  external/replex/multiplex.h
  external/replex/pes.cpp
  external/replex/pes.h
  external/replex/ringbuffer.cpp
  external/replex/ringbuffer.h
  external/replex/ts.cpp
  external/replex/ts.h
  mpeg2fix.cpp
  mpeg2fix.h
  mythtranscode.cpp
  mythtranscode_commandlineparser.cpp
  mythtranscode_commandlineparser.h
  mythtranscodeplayer.cpp
  mythtranscodeplayer.h
  transcode.cpp
  transcodedefs.h
  videodecodebuffer.cpp
  videodecodebuffer.h)

target_include_directories(mythtranscode PRIVATE .)

target_link_libraries(mythtranscode PUBLIC myth mythtv mythmpeg2)

install(TARGETS mythtranscode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
