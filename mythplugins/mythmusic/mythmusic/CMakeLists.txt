#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT TARGET PkgConfig::Vorbis)
  message(STATUS "Disabling MythMusic. Missing vorbis library.")
  return()
endif()

if(NOT TARGET PkgConfig::Flac)
  message(STATUS "Disabling MythMusic. Missing FLAC library.")
  return()
endif()

if(NOT TARGET Lame::Lame)
  message(STATUS "Disabling MythMusic. Missing LAME mp3 encoder library.")
  return()
endif()

#
# Declare the plugin
#
add_library(
  mythmusic
  avfdecoder.cpp
  avfdecoder.h
  bumpscope.cpp
  bumpscope.h
  constants.h
  decoder.cpp
  decoder.h
  decoderhandler.cpp
  decoderhandler.h
  editmetadata.cpp
  editmetadata.h
  encoder.cpp
  encoder.h
  flacencoder.cpp
  flacencoder.h
  generalsettings.cpp
  generalsettings.h
  genres.cpp
  genres.h
  importmusic.cpp
  importmusic.h
  importsettings.cpp
  importsettings.h
  inlines.h
  lameencoder.cpp
  lameencoder.h
  lyricsview.cpp
  lyricsview.h
  mainvisual.cpp
  mainvisual.h
  miniplayer.cpp
  miniplayer.h
  musiccommon.cpp
  musiccommon.h
  musicdata.cpp
  musicdata.h
  musicdbcheck.cpp
  musicdbcheck.h
  musicplayer.cpp
  musicplayer.h
  mythgoom.cpp
  mythgoom.h
  mythmusic.cpp
  playersettings.cpp
  playersettings.h
  playlist.cpp
  playlist.h
  playlistcontainer.cpp
  playlistcontainer.h
  playlisteditorview.cpp
  playlisteditorview.h
  playlistview.cpp
  playlistview.h
  pls.cpp
  pls.h
  polygon.h
  ratingsettings.cpp
  ratingsettings.h
  remoteavformatcontext.h
  searchview.cpp
  searchview.h
  smartplaylist.cpp
  smartplaylist.h
  streamview.cpp
  streamview.h
  synaesthesia.cpp
  synaesthesia.h
  visualizationsettings.cpp
  visualizationsettings.h
  visualize.cpp
  visualize.h
  visualizerview.cpp
  visualizerview.h
  vorbisencoder.cpp
  vorbisencoder.h)

if(TARGET PkgConfig::CDIO
   AND TARGET PkgConfig::CDIO_CDDA
   AND TARGET PkgConfig::CDIO_PARANOIA)
  target_sources(mythmusic PRIVATE cddecoder.h cdrip.h cddecoder.cpp cdrip.cpp)
  target_link_libraries(mythmusic PRIVATE PkgConfig::CDIO PkgConfig::CDIO_CDDA
                                          PkgConfig::CDIO_PARANOIA)
  message(STATUS "Enabling AudioCD playback support.")
  set(HAVE_CDIO TRUE)
  if(TARGET PkgConfig::MUSICBRAINZ
     AND TARGET PkgConfig::COVERART
     AND TARGET PkgConfig::DISCID)
    target_sources(mythmusic PRIVATE musicbrainz.h musicbrainz.cpp)
    target_link_libraries(mythmusic PRIVATE PkgConfig::MUSICBRAINZ PkgConfig::COVERART
                                            PkgConfig::DISCID)
    message(STATUS "Enabling MusicBrainz support.")
    set(HAVE_MUSICBRAINZ TRUE)
  else()
    message(STATUS "Disabling MusicBrainz support.")
  endif()
else()
  message(STATUS "Disabling AudioCD playback support.")
endif()

if(MINGW)
  # flac needs an additional library ...
  target_link_libraries(mythmusic PRIVATE wsock32)
endif()

#
# All remaining target information
#

target_compile_definitions(mythmusic PRIVATE MPLUGIN_API)

target_include_directories(
  mythmusic PRIVATE . $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                    ${CMAKE_INSTALL_FULL_INCLUDEDIR}/mythtv)

target_link_libraries(
  mythmusic
  PRIVATE Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Xml
          Qt${QT_VERSION_MAJOR}::OpenGL
          Lame::Lame
          PkgConfig::Flac
          PkgConfig::LIBAVCODEC
          PkgConfig::LIBAVFORMAT
          PkgConfig::LIBAVUTIL
          PkgConfig::Ogg
          PkgConfig::TagLib
          PkgConfig::Vorbis
          PkgConfig::VorbisEnc
          PkgConfig::VorbisFile
          ${BASE_MYTHTV_LIBS}
          ${MYTHMETADATA}
          ${MYTHTV})

#
# Generated files section
#

configure_file(config.h.in config.h @ONLY)

#
# Installation section
#

install(TARGETS mythmusic
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/mythtv/plugins)
