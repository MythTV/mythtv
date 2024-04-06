#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_mythsorthelper test_mythsorthelper.cpp
                                   test_mythsorthelper.h)

target_include_directories(test_mythsorthelper PRIVATE . ../..)

target_link_libraries(test_mythsorthelper PUBLIC mythbase
                                                 Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Sorting COMMAND test_mythsorthelper)
