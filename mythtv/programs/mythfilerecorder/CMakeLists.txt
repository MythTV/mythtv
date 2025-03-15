add_executable(
  mythfilerecorder
  mythfilerecorder.cpp mythfilerecorder.h
  mythfilerecorder_commandlineparser.cpp mythfilerecorder_commandlineparser.h)

target_include_directories(mythfilerecorder PRIVATE .)

target_link_libraries(mythfilerecorder PUBLIC myth mythbase mythtv)

install(TARGETS mythfilerecorder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
