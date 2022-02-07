add_executable(
  mythlcdserver
  lcdprocclient.cpp
  lcdprocclient.h
  lcdserver.cpp
  lcdserver.h
  mythlcdserver.cpp
  mythlcdserver_commandlineparser.cpp
  mythlcdserver_commandlineparser.h)

target_include_directories(mythlcdserver PRIVATE .)

target_link_libraries(mythlcdserver PUBLIC myth mythtv mythbase)

install(TARGETS mythlcdserver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
