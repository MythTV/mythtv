add_executable(
  mythpreviewgen mythpreviewgen.cpp mythpreviewgen_commandlineparser.cpp
                 mythpreviewgen_commandlineparser.h)

target_include_directories(mythpreviewgen PRIVATE .)

target_link_libraries(mythpreviewgen PUBLIC myth mythtv mythbase
         $<TARGET_NAME_IF_EXISTS:PkgConfig::VALGRIND>)

install(TARGETS mythpreviewgen RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
