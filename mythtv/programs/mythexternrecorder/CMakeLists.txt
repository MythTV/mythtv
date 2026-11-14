cmake_minimum_required(VERSION 3.20)

project(mythexternrecorder LANGUAGES CXX)

find_package(tomlplusplus CONFIG QUIET)

add_executable(
  mythexternrecorder
  config_base.h
  config_base.cpp
  config_ini.h
  config_ini.cpp
  stdinnotifier.h
  stdinnotifier.cpp
  main.cpp
  posixprocess.h
  posixprocess.cpp
  process.h
  process.cpp
  recorder.h
  recorder.cpp
  touchmonitor.h
  touchmonitor.cpp
  mythexternrecorder_commandlineparser.h
  mythexternrecorder_commandlineparser.cpp
)

if(tomlplusplus_FOUND)
  target_sources(mythexternrecorder PRIVATE
    config_toml.h
    config_toml.cpp
  )

  target_link_libraries(mythexternrecorder PRIVATE tomlplusplus::tomlplusplus)

  target_compile_definitions(mythexternrecorder PRIVATE HAVE_TOMLPLUSPLUS=1)

  message(STATUS "mythexternrecorder: tomlplusplus found. Enabling TOML support.")
else()
  message(WARNING "mythexternrecorder: tomlplusplus NOT found. Disabling TOML features.")
  target_compile_definitions(mythexternrecorder PRIVATE HAVE_TOMLPLUSPLUS=0)
endif()

target_include_directories(mythexternrecorder PRIVATE .)

target_link_libraries(mythexternrecorder PUBLIC myth mythbase mythtv)

install(TARGETS mythexternrecorder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY Examples
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv/externrecorder)
