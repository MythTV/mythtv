#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_iptvrecorder test_iptvrecorder.cpp test_iptvrecorder.h)

target_include_directories(test_iptvrecorder PRIVATE . ../..)

target_link_libraries(test_iptvrecorder PUBLIC mythtv
                                               Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME IPTvRecorder COMMAND test_iptvrecorder)
