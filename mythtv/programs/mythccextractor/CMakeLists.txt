add_executable(
  mythccextractor mythccextractor.cpp mythccextractor_commandlineparser.cpp
                  mythccextractor_commandlineparser.h)

target_include_directories(mythccextractor PRIVATE .)

target_link_libraries(mythccextractor PUBLIC myth mythtv mythbase)

install(TARGETS mythccextractor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
