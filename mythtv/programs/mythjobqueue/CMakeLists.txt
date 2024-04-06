add_executable(mythjobqueue mythjobqueue.cpp mythjobqueue_commandlineparser.cpp
                            mythjobqueue_commandlineparser.h)

target_include_directories(mythjobqueue PRIVATE .)

target_link_libraries(mythjobqueue PUBLIC myth mythtv mythbase)

install(TARGETS mythjobqueue RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
