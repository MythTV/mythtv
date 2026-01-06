#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

set(AUDIO_HEADERS
    audio/audioconvert.h
    audio/audiooutput.h
    audio/audiooutputsettings.h
    audio/audiosettings.h
    audio/eldutils.h
    audio/visualization.h
    audio/volumebase.h
    )

set(AUDIO_HEADERS_NOT_INSTALLED
    audio/audiooutputbase.h
    audio/audiooutputbase.h
    audio/audiooutputdigitalencoder.h
    audio/audiooutputnull.h
    audio/freesurround.h
    audio/freesurround_decoder.h
    audio/pink.h
    audio/spdifencoder.h
    )

#
# Define the library
#

set(LIBMYTHTV_HEADERS
    programinfo.h
    programinforemoteutil.h
    programtypes.h
    programtypeflags.h
    recordingstatus.h
    recordingtypes.h
    )

add_library(
  mythtv # cmake-format: unsort
  ${AUDIO_HEADERS_NOT_INSTALLED}
  ${AUDIO_HEADERS}
  audio/audioconvert.cpp
  audio/audiooutput.cpp
  audio/audiooutputbase.cpp
  audio/audiooutputdigitalencoder.cpp
  audio/audiooutputnull.cpp
  audio/audiooutputsettings.cpp
  audio/audiosettings.cpp
  audio/eldutils.cpp
  audio/freesurround.cpp
  audio/freesurround_decoder.cpp
  audio/pink.cpp
  audio/spdifencoder.cpp
  audio/volumebase.cpp
  ${LIBMYTHTV_HEADERS}
  bitreader.h
  bytereader.cpp
  bytereader.h
  captions/cc608decoder.cpp
  captions/cc608decoder.h
  captions/cc608reader.cpp
  captions/cc608reader.h
  captions/cc708decoder.cpp
  captions/cc708decoder.h
  captions/cc708reader.cpp
  captions/cc708reader.h
  captions/cc708window.cpp
  captions/cc708window.h
  captions/subtitlereader.cpp
  captions/subtitlereader.h
  cardutil.cpp
  cardutil.h
  channelgroup.cpp
  channelgroup.h
  channelsettings.cpp
  channelsettings.h
  dbcheck.cpp
  dbcheck.h
  driveroption.h
  io/mythavformatbuffer.cpp
  io/mythavformatbuffer.h
  io/mythfilebuffer.cpp
  io/mythfilebuffer.h
  io/mythinteractivebuffer.cpp
  io/mythinteractivebuffer.h
  io/mythmediabuffer.cpp
  io/mythmediabuffer.h
  io/mythopticalbuffer.cpp
  io/mythopticalbuffer.h
  io/mythstreamingbuffer.cpp
  io/mythstreamingbuffer.h
  jobqueue.cpp
  jobqueue.h
  listingsources.h
  livetvchain.cpp
  livetvchain.h
  metadataimagehelper.cpp
  metadataimagehelper.h
  mythavbufferref.cpp
  mythavbufferref.h
  mythaverror.cpp
  mythaverror.h
  mythavframe.h
  mythavrational.h
  mythavutil.cpp
  mythavutil.h
  mythframe.cpp
  mythframe.h
  mythhdrtracker.cpp
  mythhdrtracker.h
  mythhdrvideometadata.cpp
  mythhdrvideometadata.h
  mythsystemevent.cpp
  mythsystemevent.h
  mythtvexp.h
  playgroup.cpp
  playgroup.h
  previewgenerator.cpp
  previewgenerator.h
  previewgeneratorqueue.cpp
  previewgeneratorqueue.h
  programinfo.cpp
  programinforemoteutil.cpp
  programinfoupdater.cpp
  programinfoupdater.h
  programtypes.cpp
  recordingfile.cpp
  recordingfile.h
  recordinginfo.cpp
  recordinginfo.h
  recordingprofile.cpp
  recordingprofile.h
  recordingrule.cpp
  recordingrule.h
  recordingstatus.cpp
  recordingtypes.cpp
  remoteencoder.cpp
  remoteencoder.h
  restoredata.cpp
  restoredata.h
  scheduledrecording.cpp
  scheduledrecording.h
  signalmonitorlistener.h
  signalmonitorvalue.cpp
  signalmonitorvalue.h
  sourceutil.cpp
  sourceutil.h
  transporteditor.cpp
  transporteditor.h
  tv.cpp
  tv.h
  tvremoteutil.cpp
  tvremoteutil.h
  videodbcheck.cpp
  videodbcheck.h
  videometadatautil.cpp
  videometadatautil.h
  videosource.cpp
  videosource.h
  # DiSEqC
  diseqc.cpp
  diseqc.h
  diseqcsettings.cpp
  diseqcsettings.h
  # File/FIFO Writer classes
  io/mythavformatwriter.cpp
  io/mythavformatwriter.h
  io/mythfifowriter.cpp
  io/mythfifowriter.h
  io/mythmediawriter.cpp
  io/mythmediawriter.h
  # Teletext stuff
  captions/teletextdecoder.cpp
  captions/teletextdecoder.h
  captions/teletextreader.cpp
  captions/teletextreader.h
  captions/vbilut.cpp
  captions/vbilut.h
  # MPEG parsing stuff
  mpeg/atsc_huffman.cpp
  mpeg/atsc_huffman.h
  mpeg/atscdescriptors.cpp
  mpeg/atscdescriptors.h
  mpeg/atscstreamdata.cpp
  mpeg/atscstreamdata.h
  mpeg/atsctables.cpp
  mpeg/atsctables.h
  mpeg/AVCParser.cpp
  mpeg/AVCParser.h
  mpeg/dishdescriptors.cpp
  mpeg/dishdescriptors.h
  mpeg/dvbdescriptors.cpp
  mpeg/dvbdescriptors.h
  mpeg/dvbstreamdata.cpp
  mpeg/dvbstreamdata.h
  mpeg/dvbtables.cpp
  mpeg/dvbtables.h
  mpeg/freesat_huffman.cpp
  mpeg/freesat_huffman.h
  mpeg/freesat_tables.cpp
  mpeg/H2645Parser.cpp
  mpeg/H2645Parser.h
  mpeg/HEVCParser.cpp
  mpeg/HEVCParser.h
  mpeg/iso6937tables.cpp
  mpeg/iso6937tables.h
  mpeg/mpegdescriptors.cpp
  mpeg/mpegdescriptors.h
  mpeg/mpegstreamdata.cpp
  mpeg/mpegstreamdata.h
  mpeg/mpegtables.cpp
  mpeg/mpegtables.h
  mpeg/pespacket.cpp
  mpeg/pespacket.h
  mpeg/premieredescriptors.cpp
  mpeg/premieredescriptors.h
  mpeg/premieretables.cpp
  mpeg/premieretables.h
  mpeg/scanstreamdata.cpp
  mpeg/scanstreamdata.h
  mpeg/sctedescriptors.cpp
  mpeg/sctedescriptors.h
  mpeg/sctetables.cpp
  mpeg/sctetables.h
  mpeg/splicedescriptors.cpp
  mpeg/splicedescriptors.h
  mpeg/streamlisteners.h
  mpeg/tablestatus.cpp
  mpeg/tablestatus.h
  mpeg/tspacket.cpp
  mpeg/tspacket.h
  mpeg/tsstats.h
  mpeg/tsstreamdata.cpp
  mpeg/tsstreamdata.h
  # Channels, and the multiplexes that transmit them
  channelinfo.cpp
  channelinfo.h
  channelscan/channelimporter.cpp
  channelscan/channelimporter.h
  channelscan/iptvchannelfetcher.cpp
  channelscan/iptvchannelfetcher.h
  channelscan/scaninfo.cpp
  channelscan/scaninfo.h
  channelutil.cpp
  channelutil.h
  dtvconfparser.cpp
  dtvconfparser.h
  dtvconfparserhelpers.cpp
  dtvconfparserhelpers.h
  dtvmultiplex.cpp
  dtvmultiplex.h
  frequencies.cpp
  frequencies.h
  frequencytables.cpp
  frequencytables.h
  iptvtuningdata.cpp
  iptvtuningdata.h
  # subtitles: srt
  captions/srtwriter.cpp
  captions/srtwriter.h
  # DVD
  DVD/mythdvdbuffer.cpp
  DVD/mythdvdbuffer.h
  DVD/mythdvdcontext.cpp
  DVD/mythdvdcontext.h
  DVD/mythdvdinfo.cpp
  DVD/mythdvdinfo.h
  DVD/mythdvdstream.cpp
  DVD/mythdvdstream.h
  # Bluray
  Bluray/mythbdbuffer.cpp
  Bluray/mythbdbuffer.h
  Bluray/mythbdinfo.cpp
  Bluray/mythbdinfo.h
  Bluray/mythbdiowrapper.cpp
  Bluray/mythbdiowrapper.h
  Bluray/mythbdoverlay.cpp
  Bluray/mythbdoverlay.h
  # HLS
  HLS/httplivestream.cpp
  HLS/httplivestream.h
  HLS/httplivestreambuffer.cpp
  HLS/httplivestreambuffer.h
  HLS/m3u.cpp
  HLS/m3u.h)
if(APPLE)
  target_sources(mythtv PRIVATE util-osx.h util-osx.mm)
  if(APPLE_VIDEOTOOLBOX_LIBRARY)
    target_sources(mythtv PRIVATE decoders/mythvtbcontext.cpp
                                  decoders/mythvtbcontext.h)
  endif()
endif()
if(WIN32)
  set_source_files_properties(cardutil.cpp PROPERTIES COMPILE_DEFINITIONS
                                                      O_NONBLOCK=0)
endif(WIN32)

#
# Platform stuff
#

if(TARGET mythtv_oss)
  target_sources(mythtv PRIVATE audio/audiooutputoss.cpp audio/audiooutputoss.h)
endif()

if(TARGET PkgConfig::ALSA)
  target_sources(mythtv PRIVATE audio/audiooutputalsa.cpp audio/audiooutputalsa.h)
endif()
if(TARGET PkgConfig::JACK)
  target_sources(mythtv PRIVATE audio/audiooutputjack.cpp audio/audiooutputjack.h)
endif()
if(TARGET PkgConfig::PULSEAUDIO)
  target_sources(mythtv PRIVATE audio/audiopulsehandler.cpp
                              audio/audiopulsehandler.h)
  if(ENABLE_AUDIO_PULSEOUTPUT)
    target_sources(mythtv PRIVATE audio/audiooutputpulse.cpp
                                audio/audiooutputpulse.h)
  endif()
endif()

if(WIN32)
  target_sources(
    mythtv
    PRIVATE audio/audiooutputwin.cpp audio/audiooutputdx.cpp
            audio/audiooutputwin.h audio/audiooutputdx.h)
elseif(ANDROID)
  target_sources(
    mythtv PRIVATE audio/audiooutputopensles.cpp audio/audiooutputaudiotrack.cpp
                 audio/audiooutputopensles.h audio/audiooutputaudiotrack.h)
endif()

if(MINGW OR win32-msvc)
  target_sources(mythtv PRIVATE audio/audiooutputwin.cpp
                              audio/audiooutputdx.cpp)
  target_link_libraries(mythtv PRIVATE winmm ws2_32 user32 PkgConfig::SAMPLERATE
                                     PkgConfig::SoundTouch)
endif()

if(APPLE)
  target_sources(mythtv PRIVATE audio/audiooutputca.cpp audio/audiooutputca.h)
  target_link_libraries(
    mythtv
    PRIVATE ${APPLE_APPLICATIONSERVICES_LIBRARY} ${APPLE_AUDIOTOOLBOX_LIBRARY}
            ${APPLE_AUDIOUNIT_LIBRARY} ${APPLE_COREAUDIO_LIBRARY})
endif()

target_compile_definitions(mythtv PRIVATE MTV_API)

target_include_directories(
  mythtv
  PRIVATE .
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mythtv>)

target_link_libraries(
  mythtv
  PRIVATE
          PkgConfig::SAMPLERATE
          $<TARGET_NAME_IF_EXISTS:mythtv_oss>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::ALSA>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::JACK>
          $<TARGET_NAME_IF_EXISTS:PkgConfig::PULSEAUDIO>
          $<$<BOOL:${WIN32}>:winmm>
          $<$<BOOL:${WIN32}>:ws2_32>
          $<$<BOOL:${WIN32}>:user32>
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql
         Qt${QT_VERSION_MAJOR}::Xml
         Qt${QT_VERSION_MAJOR}::Widgets
         PkgConfig::LIBAVCODEC
         PkgConfig::LIBAVFILTER
         PkgConfig::LIBAVFORMAT
         PkgConfig::LIBAVUTIL
         PkgConfig::LIBSWRESAMPLE
         PkgConfig::LIBSWSCALE
         Iconv::Iconv
         $<TARGET_NAME_IF_EXISTS:PkgConfig::LibXml2>
         $<TARGET_NAME_IF_EXISTS:PkgConfig::SYSTEMD>
         PkgConfig::LIBCRYPTO
         PkgConfig::SoundTouch
         mythbluray
         mythbase
         mythui
         mythupnp
         mythdvdnav)

target_link_libraries(
  mythtv
  PRIVATE
  ${MYTHTV_EXTRA_LIBRARIES})

#
# Total Android Hack. The check as to whether or not libxml2 is present ends up
# deciding that iconv is builtin, and thus the Iconv::Iconv target doesn't
# provide "-liconv".  Add that missing library when necessary.
#
if(CMAKE_CROSSCOMPILING)
  if(ANDROID AND ANDROID_MIN_SDK_VERSION LESS 28)
    target_link_libraries(mythtv PUBLIC -liconv)
  endif()
endif()

if(APPLE)
  target_link_libraries(
    mythtv
    PUBLIC ${APPLE_APPLICATIONSERVICES_LIBRARY}
           ${APPLE_COCOA_LIBRARY}
           ${APPLE_COREFOUNDATION_LIBRARY}
           ${APPLE_CORESERVICES_LIBRARY}
           ${APPLE_COREVIDEO_LIBRARY}
           ${APPLE_IOKIT_LIBRARY}
           ${APPLE_IOSURFACE_LIBRARY}
           ${APPLE_OPENGL_LIBRARY}
           ${APPLE_VIDEOTOOLBOX_LIBRARY})
endif()

# ====
#
# FRONTEND OR BACKEND ADDITIONS
#
# ====
if(ENABLE_FRONTEND OR ENABLE_BACKEND)
  if(TARGET PkgConfig::V4L2)
    target_link_libraries(mythtv PRIVATE PkgConfig::V4L2)
    target_sources(mythtv PRIVATE v4l2util.cpp v4l2util.h)
  endif()
  if(ENABLE_FRONTEND)
    include(cmake/Frontend.cmake)
  endif()
  if(ENABLE_BACKEND)
    include(cmake/Backend.cmake)
  endif()
endif()

#
# Installation section
#

set(HEADERS_TO_INSTALL1
    metadataimagehelper.h
    mythaverror.h
    mythavframe.h
    mythavutil.h
    mythframe.h
    mythtvexp.h
    playgroup.h
    )

set(HEADERS_TO_INSTALL2
    visualisations/goom/drawmethods.h
    visualisations/goom/filters.h
    visualisations/goom/goom_core.h
    visualisations/goom/goom_tools.h
    visualisations/goom/goomconfig.h
    visualisations/goom/graphic.h
    visualisations/goom/ifs.h
    visualisations/goom/lines.h
    visualisations/goom/mathtools.h
    visualisations/goom/mmx.h
    visualisations/goom/tentacle3d.h
    visualisations/goom/v3d.h)

install(TARGETS mythtv LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES ${HEADERS_TO_INSTALL1} ${LIBMYTHTV_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythtv)

install(
  FILES ${HEADERS_TO_INSTALL2}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythtv/visualisations/goom)

install(FILES ${AUDIO_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mythtv/libmythtv/audio)
