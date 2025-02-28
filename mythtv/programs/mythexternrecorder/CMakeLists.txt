add_executable(
  mythexternrecorder
  MythExternControl.cpp
  MythExternControl.h
  MythExternRecApp.cpp
  MythExternRecApp.h
  mythexternrecorder.cpp
  mythexternrecorder_commandlineparser.cpp
  mythexternrecorder_commandlineparser.h)

target_include_directories(mythexternrecorder PRIVATE .)

target_link_libraries(mythexternrecorder PUBLIC myth mythbase mythtv)

install(TARGETS mythexternrecorder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY config/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/externrecorder)
