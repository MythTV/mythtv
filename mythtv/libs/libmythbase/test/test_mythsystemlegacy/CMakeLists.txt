#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythsystemlegacy test_mythsystemlegacy.cpp
                                     test_mythsystemlegacy.h)

target_include_directories(test_mythsystemlegacy PRIVATE . ../..)

target_link_libraries(test_mythsystemlegacy PUBLIC mythbase
                                                   Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME CallSystemLegacy COMMAND test_mythsystemlegacy)
