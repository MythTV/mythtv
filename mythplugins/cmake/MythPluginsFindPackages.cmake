#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

#
# DEBUGGING - REMOVE ME
#
# include(CMakePrintSystemInformation)
include(CMakePrintHelpers)

#
# This module needs pkg-config functionality
#
find_package(PkgConfig REQUIRED)

#
# Find libraries used by the mythgame plugin.
#
find_package(ZLIB REQUIRED)
pkg_check_modules(LIBZIP "libzip" REQUIRED IMPORTED_TARGET)

#
# Find libraries used by the mythmusic plugin.
#
pkg_check_modules(CDIO "libcdio" IMPORTED_TARGET)
if(TARGET PkgConfig::CDIO)
  pkg_check_modules(CDIO_CDDA libcdio_cdda IMPORTED_TARGET)
  pkg_check_modules(CDIO_PARANOIA libcdio_paranoia IMPORTED_TARGET)
  pkg_search_module(MUSICBRAINZ libmusicbrainz5cc libmusicbrainz5 IMPORTED_TARGET)
  if(MUSICBRAINZ_FOUND)
    message(STATUS "  Found ${MUSICBRAINZ_MODULE_NAME}, version ${MUSICBRAINZ_VERSION}")
  endif()
  pkg_search_module(COVERART libcoverartcc libcoverart IMPORTED_TARGET)
  if(COVERART_FOUND)
    message(STATUS "  Found ${COVERART_MODULE_NAME}, version ${COVERART_VERSION}")
  endif()
  pkg_check_modules(DISCID libdiscid IMPORTED_TARGET)
endif()

pkg_check_modules(Ogg "ogg" IMPORTED_TARGET)
pkg_check_modules(Vorbis "vorbis" IMPORTED_TARGET)
pkg_check_modules(VorbisEnc "vorbisenc" IMPORTED_TARGET)
pkg_check_modules(VorbisFile "vorbisfile" IMPORTED_TARGET)
pkg_check_modules(Flac "flac" IMPORTED_TARGET)
pkg_check_modules(TagLib "taglib" IMPORTED_TARGET)

if(ENABLE_MP3LAME)
  find_package(Lame 3.98.3)
  if(TARGET Lame::Lame)
    target_compile_definitions(Lame::Lame INTERFACE USING_LIBMP3LAME)
  endif()
endif()

find_package(
  Python3 3.6
  COMPONENTS Interpreter
  REQUIRED)
if(TARGET Python3::Interpreter)
  find_package(Python3Modules OPTIONAL_COMPONENTS MythTV lxml oauth pycurl
                                                  urllib xml)
endif()

if(NOT TARGET Python3::Interpreter)
  message(STATUS "Missing python interpreter. Disabling netvision.")
elseif(Python3Modules_COMPONENTS_MISSING)
  message(STATUS "Missing some python modules. Disabling netvision.")
else()
  set(ENABLE_NETVISION TRUE)
endif()

find_package(Perl)
if(PERL_FOUND)
  find_package(
    PerlModules
    OPTIONAL_COMPONENTS
      Date::Manip
      DateTime::Format::ISO8601
      Image::Size
      JSON
      SOAP::Lite
      XML::Simple
      XML::XPath)
endif()
if(NOT PERL_FOUND)
  message(STATUS "Missing perl interpreter. Disabling mythweather.")
elseif(PerlModules_COMPONENTS_MISSING)
  message(STATUS "Missing some perl modules. Disabling mythweather.")
else()
  set(ENABLE_WEATHER TRUE)
endif()
