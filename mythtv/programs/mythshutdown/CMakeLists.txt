add_executable(mythshutdown mythshutdown.cpp mythshutdown_commandlineparser.cpp
                            mythshutdown_commandlineparser.h)

target_include_directories(mythshutdown PRIVATE .)

target_link_libraries(mythshutdown PUBLIC myth mythtv mythbase)

install(TARGETS mythshutdown RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
