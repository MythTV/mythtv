#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT CMAKE_CROSSCOMPILING)
  return()
endif()

if(NOT PLATFORM_CONFIGURE_ENV)
  message(
    FATAL_ERROR
      "Don't know compiler flags to use for this type of cross compile.")
endif()

#
# Set some convenience variables.
#
if(VERBOSE)
  set(CONFIGURE_VERBOSE_ARG "--enable-silent-rules")
else()
  set(CONFIGURE_VERBOSE_ARG "--disable-silent-rules")
endif()

if(WIN32 AND HOST_LSB_RELEASE_ID MATCHES "fedora")
  set(PREBUILT_TAG "(also available as a pre-built package)")
endif()

# ##############################################################################
#
# External projects start here.
#
# ##############################################################################

if(CMAKE_CROSSCOMPILING AND CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
  message(
    FATAL_ERROR "CMAKE_INSTALL_PREFIX is /usr/local while cross compiling.")
endif()

include(externallibs/FindOrBuildZlib)
if(WIN32)
  include(externallibs/FindOrBuildDlfcn)
  include(externallibs/FindOrBuildMesa)
endif()
include(externallibs/FindOrBuildPCRE2)
include(externallibs/FindOrBuildLzo)
include(externallibs/FindOrBuildZip)
include(externallibs/FindOrBuildTaglib)
include(externallibs/FindOrBuildFreetype)
include(externallibs/FindOrBuildOpenSSL)
include(externallibs/FindOrBuildIconv)
include(externallibs/FindOrBuildMariaDBConnector)
include(externallibs/FindOrBuildLame)
include(externallibs/FindOrBuildOgg)
include(externallibs/FindOrBuildVorbis)
include(externallibs/FindOrBuildFlac)
include(externallibs/FindOrBuildXml2)
include(externallibs/FindOrBuildSamplerate)
include(externallibs/FindOrBuildSoundTouch)
include(externallibs/FindOrBuildFontconfig)
include(externallibs/FindOrBuildFribidi)
include(externallibs/FindOrBuildICU)
include(externallibs/FindOrBuildHarfbuzz)
include(externallibs/FindOrBuildAss)
include(externallibs/FindOrBuildBluray)
set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})
include(externallibs/BuildQt${QT_VERSION_MAJOR}
        RESULT_VARIABLE included_file_name)
if(NOT included_file_name)
  message(FATAL_ERROR "Don't know how to build Qt${QT_VERSION_MAJOR}")
endif()

#
# Find or build each of the required external libraries.
#
find_or_build_zlib()
if(WIN32)
  find_or_build_dlfcn()
  find_or_build_mesa()
endif()
find_or_build_pcre2()
find_or_build_lzo()
find_or_build_libzip()
find_or_build_taglib()
find_or_build_freetype()
find_or_build_openssl()
find_or_build_iconv()
find_or_build_maria_db_connector()
find_or_build_lame()
find_or_build_ogg()
find_or_build_vorbis()
find_or_build_flac()
find_or_build_libxml2()
find_or_build_samplerate()
find_or_build_soundtouch()
find_or_build_fontconfig()
find_or_build_fribidi()
find_or_build_icu()
find_or_build_harfbuzz()
find_or_build_libass()
find_or_build_libbluray()
find_or_build_qt()
