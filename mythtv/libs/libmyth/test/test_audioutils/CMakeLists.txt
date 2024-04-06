#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_audioutils test_audioutils.cpp test_audioutils.h)

target_include_directories(test_audioutils PRIVATE . ../..)

target_link_libraries(test_audioutils PUBLIC myth Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME AudioUtils COMMAND test_audioutils)
