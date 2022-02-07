#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_eitfixups test_eitfixups.cpp test_eitfixups.h)

target_include_directories(test_eitfixups PRIVATE . ../..)

target_link_libraries(test_eitfixups PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME EIT_Fixups COMMAND test_eitfixups)
