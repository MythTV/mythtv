#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT PROJECT_VERSION)
  message(FATAL_ERROR "This file must be included after the project command.")
endif()
if(NOT QT_VERSION_MAJOR)
  message(FATAL_ERROR "The QT_VERSION_MAJOR variable must be set.")
endif()

#
# Declare parameters passed to sub-builds.  All version specific MythTV values
# should be declared here and in the superbuild version number.
#

# See mythtv/libs/libmythbase/mythversion.h.in
set(MYTHTV_BINARY_CHANGED "20250212-1")
set(MYTHTV_BINARY_VERSION "${PROJECT_VERSION_MAJOR}.${MYTHTV_BINARY_CHANGED}")

# See mythtv/bindings/python/MythTV/static.py.in
set(MYTHTV_PYTHON_OWN_VERSION "(${PROJECT_VERSION_MAJOR},0,-1,0)")

# See mythtv/bindings/python/MythTV/services_api/mythversions.py.in
set(MYTHTV_PYTHON_VERSION_LIST "('0.27', '0.28'")
foreach(_V RANGE 29 ${PROJECT_VERSION_MAJOR})
  string(APPEND MYTHTV_PYTHON_VERSION_LIST ", '${_V}'")
endforeach()
string(APPEND MYTHTV_PYTHON_VERSION_LIST ")")

# See mythtv/bindings/perl/Makefile.PL.in, mythtv/bindings/perl/MythTV.pm, and
# mythtv/bindings/python/setup.cfg.in
set(MYTHTV_VERSION_MAJMIN ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

#
# Qt Minimum Versions
#
include(VersionToNumber)
set(QT5_MIN_VERSION_STR "5.15.2")
set(QT6_MIN_VERSION_STR "6.4.0")
version_to_number(QT_MIN_VERSION ${QT${QT_VERSION_MAJOR}_MIN_VERSION_STR} FALSE)
version_to_number(QT_MIN_VERSION_HEX ${QT${QT_VERSION_MAJOR}_MIN_VERSION_STR}
                  TRUE)
set(QT_PKG_NAME Qt${QT_VERSION_MAJOR})
set(QT_PKG_NAME_UC QT${QT_VERSION_MAJOR})
set(QT_PKG_NAME_LC qt${QT_VERSION_MAJOR})
