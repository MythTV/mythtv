#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

add_executable(
  test_mheg_dsmcc
  ../../mheg/dsmcc.cpp ../../mheg/dsmccbiop.cpp ../../mheg/dsmcccache.cpp
  ../../mheg/dsmccobjcarousel.cpp test_mheg_dsmcc.cpp test_mheg_dsmcc.h)

target_include_directories(test_mheg_dsmcc PRIVATE . ../..)

target_link_libraries(test_mheg_dsmcc PUBLIC mythtv Qt${QT_VERSION_MAJOR}::Test)

add_test(NAME MhegDsmcc COMMAND test_mheg_dsmcc)
