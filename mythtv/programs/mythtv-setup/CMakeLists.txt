add_executable(
  mythtvsetup
  backendsettings.cpp
  backendsettings.h
  channeleditor.cpp
  channeleditor.h
  checksetup.cpp
  checksetup.h
  exitprompt.cpp
  exitprompt.h
  expertsettingseditor.h
  importicons.cpp
  importicons.h
  mythtv-setup.cpp
  mythtv-setup_commandlineparser.cpp
  mythtv-setup_commandlineparser.h
  startprompt.cpp
  startprompt.h)

target_include_directories(mythtvsetup PRIVATE .)

target_link_libraries(mythtvsetup PUBLIC myth mythtv mythbase mythui mythupnp)

install(TARGETS mythtvsetup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES setup.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv)
