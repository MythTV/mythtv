#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(test_rssparse test_rssparse.cpp test_rssparse.h)

target_include_directories(test_rssparse PRIVATE . ../..)

target_link_libraries(test_rssparse PUBLIC myth Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME RssParse COMMAND test_rssparse)
