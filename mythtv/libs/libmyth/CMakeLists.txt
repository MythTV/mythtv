#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

#
# Enumerate all headers
#
set(LIBMYTH_HEADERS
    langsettings.h
    mythaverror.h
    mythcontext.h
    mythexp.h
    mythmediamonitor.h
    mythterminal.h
    output.h
    schemawizard.h
    standardsettings.h
    storagegroupeditor.h
    visual.h)

set(LIBMYTH_HEADERS_NOT_INSTALLED backendselect.h dbsettings.h guistartup.h
                                  rawsettingseditor.h)

set(AUDIO_HEADERS
    audio/audioconvert.h
    audio/audiooutput.h
    audio/audiooutputsettings.h
    audio/audiooutpututil.h
    audio/audiosettings.h
    audio/eldutils.h
    audio/volumebase.h)

set(AUDIO_HEADERS_NOT_INSTALLED
    audio/audiooutputbase.h
    audio/audiooutputbase.h
    audio/audiooutputdigitalencoder.h
    audio/audiooutputdownmix.h
    audio/audiooutputgraph.h
    audio/audiooutputnull.h
    audio/pink.h
    audio/spdifencoder.h)

#
# Declare the library
#
add_library(
  myth
  # Include headers here so the they will be scanned for Qt classes
  # and potentially have MOC invoked on them.
  ${AUDIO_HEADERS_NOT_INSTALLED}
  ${AUDIO_HEADERS}
  ${LIBMYTH_HEADERS_NOT_INSTALLED}
  ${LIBMYTH_HEADERS}
  audio/audioconvert.cpp
  audio/audiooutput.cpp
  audio/audiooutputbase.cpp
  audio/audiooutputdigitalencoder.cpp
  audio/audiooutputdownmix.cpp
  audio/audiooutputgraph.cpp
  audio/audiooutputnull.cpp
  audio/audiooutputsettings.cpp
  audio/audiooutpututil.cpp
  audio/audiosettings.cpp
  audio/eldutils.cpp
  audio/pink.cpp
  audio/spdifencoder.cpp
  audio/volumebase.cpp
  backendselect.cpp
  dbsettings.cpp
  guistartup.cpp
  langsettings.cpp
  mythaverror.cpp
  mythcontext.cpp
  mythmediamonitor.cpp
  mythterminal.cpp
  output.cpp
  rawsettingseditor.cpp
  schemawizard.cpp
  standardsettings.cpp
  storagegroupeditor.cpp)

#
# Platform stuff
#

if(TARGET mythtv_oss)
  target_sources(myth PRIVATE audio/audiooutputoss.cpp audio/audiooutputoss.h)
endif()

if(TARGET PkgConfig::ALSA)
  target_sources(myth PRIVATE audio/audiooutputalsa.cpp audio/audiooutputalsa.h)
endif()
if(TARGET PkgConfig::JACK)
  target_sources(myth PRIVATE audio/audiooutputjack.cpp audio/audiooutputjack.h)
endif()
if(TARGET PkgConfig::PULSEAUDIO)
  target_sources(myth PRIVATE audio/audiopulsehandler.cpp
                              audio/audiopulsehandler.h)
  if(ENABLE_AUDIO_PULSEOUTPUT)
    target_sources(myth PRIVATE audio/audiooutputpulse.cpp
                                audio/audiooutputpulse.h)
  endif()
endif()
if(UNIX)
  target_sources(myth PRIVATE mediamonitor-unix.cpp mediamonitor-unix.h)
endif()
if(WIN32)
  target_sources(
    myth
    PRIVATE mediamonitor-windows.cpp mediamonitor-windows.h
            audio/audiooutputwin.cpp audio/audiooutputdx.cpp
            audio/audiooutputwin.h audio/audiooutputdx.h)
elseif(ANDROID)
  target_sources(
    myth PRIVATE audio/audiooutputopensles.cpp audio/audiooutputaudiotrack.cpp
                 audio/audiooutputopensles.h audio/audiooutputaudiotrack.h)
endif()

if(MINGW OR win32-msvc)
  target_sources(myth PRIVATE mediamonitor-windows.cpp audio/audiooutputwin.cpp
                              audio/audiooutputdx.cpp)
  target_link_libraries(myth PRIVATE winmm ws2_32 user32 PkgConfig::SAMPLERATE
                                     PkgConfig::SoundTouch)
endif()

if(APPLE)
  target_sources(myth PRIVATE audio/audiooutputca.cpp audio/audiooutputca.h)
  target_link_libraries(
    myth
    PRIVATE ${APPLE_APPLICATIONSERVICES_LIBRARY} ${APPLE_AUDIOTOOLBOX_LIBRARY}
            ${APPLE_AUDIOUNIT_LIBRARY} ${APPLE_COREAUDIO_LIBRARY}
            ${APPLE_IOKIT_LIBRARY})
  if(DARWIN_DA)
    set_target_properties(myth PROPERTIES COMPILE_DEFINITIONS USING_DARWIN_DA)
    target_sources(myth PRIVATE mediamonitor-darwin.cpp mediamonitor-darwin.h)
    target_link_libraries(myth PRIVATE ${APPLE_DISKARBITRATION_LIBRARY})
  endif()
endif()

#
# All remaining target information
#

set_source_files_properties(mediamonitor-unix.h PROPERTIES SKIP_AUTOMOC TRUE)

target_compile_definitions(myth PRIVATE MYTH_API)

target_include_directories(
  myth
  PRIVATE .
  PUBLIC ${PROJECT_SOURCE_DIR}/libs)

target_link_libraries(
  myth
  PRIVATE Qt${QT_VERSION_MAJOR}::Core
          Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Sql
          Qt${QT_VERSION_MAJOR}::Xml
          Qt${QT_VERSION_MAJOR}::Widgets
          PkgConfig::SAMPLERATE
          PkgConfig::SoundTouch
          $<TARGET_NAME_IF_EXISTS:mythtv_oss>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::ALSA>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::JACK>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::PULSEAUDIO>
          $<TARGET_NAME_IF_EXISTS:Qt${QT_VERSION_MAJOR}::DBus>
          $<$<BOOL:${WIN32}>:winmm>
          $<$<BOOL:${WIN32}>:ws2_32>
          $<$<BOOL:${WIN32}>:user32>
  PUBLIC mythbase
         mythui
         mythupnp
         mythfreesurround
         mythbluray
         PkgConfig::LIBAVCODEC
         PkgConfig::LIBAVFORMAT
         PkgConfig::LIBAVUTIL
         PkgConfig::LIBSWRESAMPLE)

#
# Installation section
#

install(TARGETS myth LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${LIBMYTH_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmyth)

install(FILES ${AUDIO_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmyth/audio)
