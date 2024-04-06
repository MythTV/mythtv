add_executable(
  mythmediaserver
  controlrequesthandler.cpp controlrequesthandler.h mythmediaserver.cpp
  mythmediaserver_commandlineparser.cpp mythmediaserver_commandlineparser.h)

target_include_directories(mythmediaserver PRIVATE .)

target_link_libraries(mythmediaserver PUBLIC myth mythtv mythbase
                                             mythprotoserver)

install(TARGETS mythmediaserver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
