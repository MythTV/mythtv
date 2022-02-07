add_executable(mythavtest mythavtest.cpp mythavtest_commandlineparser.cpp
                          mythavtest_commandlineparser.h)

target_include_directories(mythavtest PRIVATE .)

target_link_libraries(mythavtest PUBLIC myth mythtv mythbase mythui)

install(TARGETS mythavtest RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
