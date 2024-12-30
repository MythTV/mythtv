#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_settings test_settings.cpp test_settings.h)

target_include_directories(test_settings PRIVATE . ../..)

target_link_libraries(test_settings PUBLIC mythui Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Settings COMMAND test_settings)
