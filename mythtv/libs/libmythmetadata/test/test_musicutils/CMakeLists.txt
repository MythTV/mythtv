#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_musicutils test_musicutils.cpp test_musicutils.h)

target_include_directories(test_musicutils PRIVATE . ../..)

target_link_libraries(test_musicutils PUBLIC mythmetadata
                                             Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MusicUtils COMMAND test_musicutils)
