#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_template test_template.cpp test_template.h)

target_include_directories(test_template PRIVATE . ../..)

target_link_libraries(test_template PUBLIC mythbase Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Template COMMAND test_template)
