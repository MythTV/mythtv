#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythsystem test_mythsystem.cpp test_mythsystem.h)

target_include_directories(test_mythsystem PRIVATE . ../..)

target_link_libraries(test_mythsystem PUBLIC mythbase
                                             Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME CallSystem COMMAND test_mythsystem)
