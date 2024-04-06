#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_programinfo test_programinfo.cpp test_programinfo.h)

target_include_directories(test_programinfo PRIVATE . ../..)

target_link_libraries(test_programinfo PUBLIC myth Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME AudioConvert COMMAND test_programinfo)
