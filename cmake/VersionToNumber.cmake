#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

function(version_to_number out version hex)
  if(hex)
    set(_format HEXA)
  endif()
  if(version MATCHES "^([0-9]+)\.([0-9]+)(\.([0-9]+))?")
    math(EXPR _value
         "${CMAKE_MATCH_1} * 65536 + ${CMAKE_MATCH_2} * 256 + 0${CMAKE_MATCH_4}"
         OUTPUT_FORMAT ${_format}DECIMAL)
  else()
    message(FATAL_ERROR "Can't parse ${version} into maj/min/patch")
  endif()
  set(${out}
      ${_value}
      PARENT_SCOPE)
endfunction()

if(ENABLE_SELF_TEST)
  version_to_number(TEST_DECIMAL "5.12.1" FALSE)
  version_to_number(TEST_HEX "5.12.1" TRUE)
  if(NOT TEST_DECIMAL EQUAL 330753)
    message(FATAL_ERROR "version_to_number decimal test failed")
  elseif(NOT TEST_HEX EQUAL 0x050C01)
    message(FATAL_ERROR "version_to_number hexadecimal test failed")
  endif()
endif()
