add_executable(
  mythutil
  backendutils.cpp
  backendutils.h
  eitutils.cpp
  eitutils.h
  fileutils.cpp
  fileutils.h
  jobutils.cpp
  jobutils.h
  markuputils.cpp
  markuputils.h
  messageutils.cpp
  messageutils.h
  mpegutils.cpp
  mpegutils.h
  musicmetautils.cpp
  musicmetautils.h
  mythutil.cpp
  mythutil.h
  mythutil_commandlineparser.cpp
  mythutil_commandlineparser.h
  recordingutils.cpp
  recordingutils.h)

target_include_directories(mythutil PRIVATE .)

target_link_libraries(mythutil PUBLIC myth mythtv mythbase mythmetadata)

install(TARGETS mythutil RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
