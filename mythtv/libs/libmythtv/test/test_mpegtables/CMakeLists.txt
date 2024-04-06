#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mpegtables specimen.cpp test_mpegtables.cpp
                               test_mpegtables.h)

target_include_directories(test_mpegtables PRIVATE . ../..)

target_link_libraries(test_mpegtables PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MpegTables COMMAND test_mpegtables)
