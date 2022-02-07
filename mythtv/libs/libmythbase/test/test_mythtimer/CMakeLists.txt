#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythtimer test_mythtimer.cpp test_mythtimer.h)

target_include_directories(test_mythtimer PRIVATE . ../..)

target_link_libraries(test_mythtimer PUBLIC mythbase
                                            Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Timer COMMAND test_mythtimer)
