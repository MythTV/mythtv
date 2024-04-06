add_executable(
  mythpreviewgen mythpreviewgen.cpp mythpreviewgen_commandlineparser.cpp
                 mythpreviewgen_commandlineparser.h)

target_include_directories(mythpreviewgen PRIVATE .)

target_compile_definitions(mythpreviewgen
                           PRIVATE $<$<BOOL:${USING_VALGRIND}>:USING_VALGRIND>)

target_link_libraries(mythpreviewgen PUBLIC myth mythtv mythbase)

install(TARGETS mythpreviewgen RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
