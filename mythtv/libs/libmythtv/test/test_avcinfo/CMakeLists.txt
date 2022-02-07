#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_avcinfo test_avcinfo.cpp test_avcinfo.h)

target_include_directories(test_avcinfo PRIVATE . ../..)

target_link_libraries(test_avcinfo PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME AVCInfo COMMAND test_avcinfo)
