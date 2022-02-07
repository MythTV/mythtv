#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_copyframes test_copyframes.cpp test_copyframes.h)

target_include_directories(test_copyframes PRIVATE . ../..)

target_link_libraries(test_copyframes PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME CopyFrames COMMAND test_copyframes)
