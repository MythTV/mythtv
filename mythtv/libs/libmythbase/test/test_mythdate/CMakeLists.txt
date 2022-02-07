#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythdate test_mythdate.cpp test_mythdate.h)

target_include_directories(test_mythdate PRIVATE . ../..)

target_link_libraries(test_mythdate PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME DateFormatting COMMAND test_mythdate)
