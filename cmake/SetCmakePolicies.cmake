#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

# The setting of cmake minimum version 3.20 means that all policy numbers less
# than or equal to CMP0120 are set to NEW.
#
# https://cmake.org/cmake/help/latest/manual/cmake-policies.7.html#policies-introduced-by-cmake-3-20

# ~~~
# https://cmake.org/cmake/help/latest/policy/CMP0126.html
#
# When this policy is set to NEW, the set(CACHE) command does not
# remove any normal variable of the same name from the current scope.
# ~~~
if(POLICY CMP0126)
  cmake_policy(SET CMP0126 NEW)
endif()

# ~~~
# https://cmake.org/cmake/help/latest/policy/CMP0135.html
# OLD: Use timestamps from downloaded file.
# NEW: Set timestamps of extracted content to current time.
#
# This messes up the projects that are based on autotools: fribidi,
# libass, and libsamplerate. Once cmake 3.24 becomes the minimum,
# uncomment the "DOWNLOAD_EXTRACT_TIMESTAMP ON" line in their External
# Project declaration, then this setting won't matter any more.
# ~~~
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 OLD)
endif()
