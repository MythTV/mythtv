add_executable(
  mythtv-setup
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

target_include_directories(mythtv-setup PRIVATE .)

target_link_libraries(mythtv-setup PUBLIC myth mythtv mythbase mythui mythupnp)

install(TARGETS mythtv-setup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES setup.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv)
