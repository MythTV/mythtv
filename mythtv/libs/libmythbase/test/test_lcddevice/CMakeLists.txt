#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_lcddevice test_lcddevice.cpp test_lcddevice.h)

target_include_directories(test_lcddevice PRIVATE . ../..)

target_link_libraries(test_lcddevice PUBLIC mythbase
                                            Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME LCD COMMAND test_lcddevice)
