add_executable(
  mythmetadatalookup
  lookup.cpp lookup.h mythmetadatalookup.cpp
  mythmetadatalookup_commandlineparser.cpp
  mythmetadatalookup_commandlineparser.h)

target_include_directories(mythmetadatalookup PRIVATE .)

target_link_libraries(mythmetadatalookup PUBLIC myth mythtv mythbase
                                                mythmetadata)

install(TARGETS mythmetadatalookup RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
