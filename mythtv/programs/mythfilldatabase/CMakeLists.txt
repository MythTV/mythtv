add_executable(
  mythfilldatabase
  channeldata.cpp
  channeldata.h
  filldata.cpp
  filldata.h
  fillutil.cpp
  fillutil.h
  mythfilldatabase.cpp
  mythfilldatabase_commandlineparser.cpp
  mythfilldatabase_commandlineparser.h
  xmltvparser.cpp
  xmltvparser.h)

target_include_directories(mythfilldatabase PRIVATE .)

target_link_libraries(mythfilldatabase PUBLIC myth mythtv mythbase mythmetadata)

install(TARGETS mythfilldatabase RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
