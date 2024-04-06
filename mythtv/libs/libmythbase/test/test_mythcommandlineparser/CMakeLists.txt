#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythcommandlineparser test_mythcommandlineparser.cpp
                                          test_mythcommandlineparser.h)

target_include_directories(test_mythcommandlineparser PRIVATE . ../..)

target_link_libraries(test_mythcommandlineparser
                      PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME CommandLineParser COMMAND test_mythcommandlineparser)
