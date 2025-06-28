#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_bitreader test_bitreader.cpp test_bitreader.h)

target_include_directories(test_bitreader PRIVATE . ../..)

target_link_libraries(test_bitreader PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME BitReader COMMAND test_bitreader)
