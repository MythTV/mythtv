#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_logging test_logging.cpp test_logging.h)

target_include_directories(test_logging PRIVATE . ../..)

target_link_libraries(test_logging PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Logging COMMAND test_logging)
