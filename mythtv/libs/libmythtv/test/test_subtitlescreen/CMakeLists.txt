#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_subtitlescreen test_subtitlescreen.cpp
                                   test_subtitlescreen.h)

target_include_directories(test_subtitlescreen PRIVATE . ../..)

target_link_libraries(test_subtitlescreen PUBLIC mythtv
                                                 Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME SubtitleScreen COMMAND test_subtitlescreen)
