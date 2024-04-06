#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_iso639 test_iso639.cpp test_iso639.h)

target_include_directories(test_iso639 PRIVATE . ../..)

target_link_libraries(test_iso639 PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME ISO639 COMMAND test_iso639)
