add_executable(
  mythscreenwizard
  mythscreenwizard.cpp mythscreenwizard_commandlineparser.cpp
  mythscreenwizard_commandlineparser.h screenwizard.cpp screenwizard.h)

target_include_directories(mythscreenwizard PRIVATE .)

target_link_libraries(mythscreenwizard PUBLIC myth mythtv mythbase mythui)

install(TARGETS mythscreenwizard RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
