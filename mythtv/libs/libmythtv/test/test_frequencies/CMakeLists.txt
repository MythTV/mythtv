#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(
  test_frequencies
  ../../mheg/dsmcc.cpp ../../mheg/dsmccbiop.cpp ../../mheg/dsmcccache.cpp
  ../../mheg/dsmccobjcarousel.cpp test_frequencies.cpp test_frequencies.h)

target_include_directories(test_frequencies PRIVATE . ../..)

target_link_libraries(test_frequencies PUBLIC mythtv
                                              Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME Freqencies COMMAND test_frequencies)
