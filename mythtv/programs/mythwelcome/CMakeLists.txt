add_executable(
  mythwelcome
  mythwelcome.cpp
  mythwelcome_commandlineparser.cpp
  mythwelcome_commandlineparser.h
  welcomedialog.cpp
  welcomedialog.h
  welcomesettings.cpp
  welcomesettings.h)

target_include_directories(mythwelcome PRIVATE .)

if(WIN32)
  target_compile_definitions(mythwelcome PRIVATE WINDOWS_CLOSE_CONSOLE)
endif()

target_link_libraries(mythwelcome PUBLIC myth mythtv mythbase mythui)

install(TARGETS mythwelcome RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
