#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(NOT ENABLE_BACKEND)
  return()
endif()

target_link_libraries(mythtv PUBLIC PkgConfig::LZO2)
target_compile_definitions(
  mythtv
  PUBLIC USING_IPTV
  PRIVATE USING_BACKEND)
target_sources(
  mythtv
  PRIVATE # Channel stuff
          recorders/channelbase.h
          recorders/dtvchannel.h
          recorders/signalmonitor.h
          recorders/dtvsignalmonitor.h
          recorders/scriptsignalmonitor.h
          recorders/channelbase.cpp
          recorders/dtvchannel.cpp
          recorders/dtvsignalmonitor.cpp
          recorders/signalmonitor.cpp
          inputinfo.h
          inputinfo.cpp
          # Channel scanner stuff
          scanwizard.h
          scanwizard.cpp
          channelscan/channelscan_sm.h
          channelscan/channelscanner.h
          channelscan/channelscanner_cli.h
          channelscan/channelscanner_gui.h
          channelscan/channelscanner_gui_scan_pane.h
          channelscan/channelscanner_web.cpp
          channelscan/channelscanner_web.h
          channelscan/frequencytablesetting.h
          channelscan/inputselectorsetting.h
          channelscan/channelscanmiscsettings.h
          channelscan/modulationsetting.h
          channelscan/multiplexsetting.h
          channelscan/paneanalog.h
          channelscan/paneatsc.h
          channelscan/panedvbc.h
          channelscan/panedvbs.h
          channelscan/panedvbs2.h
          channelscan/panedvbt.h
          channelscan/panedvbt2.h
          channelscan/panedvbutilsimport.h
          channelscan/panesingle.h
          channelscan/scanmonitor.h
          channelscan/scanwizardconfig.h
          channelscan/channelscan_sm.cpp
          channelscan/channelscanner.cpp
          channelscan/channelscanner_cli.cpp
          channelscan/channelscanner_gui.cpp
          channelscan/channelscanner_gui_scan_pane.cpp
          channelscan/frequencytablesetting.cpp
          channelscan/inputselectorsetting.cpp
          channelscan/multiplexsetting.cpp
          channelscan/paneanalog.cpp
          channelscan/scanmonitor.cpp
          channelscan/scanwizardconfig.cpp
          # EIT stuff
          eithelper.h
          eitscanner.h
          eitfixup.h
          eitcache.h
          eitcache.cpp
          eitfixup.cpp
          eithelper.cpp
          eitscanner.cpp
          # non-EIT EPG stuff
          programdata.h
          programdata.cpp
          # TVRec stuff
          tv_rec.h
          recordingquality.h
          recordingquality.cpp
          tv_rec.cpp
          # Recorder base and util classes
          recorders/recorderbase.h
          recorders/DeviceReadBuffer.h
          recorders/dtvrecorder.h
          recorders/DeviceReadBuffer.cpp
          recorders/dtvrecorder.cpp
          recorders/recorderbase.cpp
          # Import recorder
          recorders/importrecorder.h
          recorders/importrecorder.cpp
          recorders/RTjpegN.h
          recorders/audioinput.h
          recorders/go7007_myth.h
          recorders/RTjpegN.cpp
          recorders/audioinput.cpp
          # Support for RTP/UDP streams
          recorders/cetonrtsp.h
          recorders/iptvchannel.h
          recorders/iptvrecorder.h
          recorders/iptvsignalmonitor.h
          recorders/iptvstreamhandler.h
          recorders/streamhandler.h
          recorders/rtp/udppacket.h
          recorders/rtp/udppacketbuffer.h
          recorders/rtp/packetbuffer.h
          recorders/rtp/rtppacketbuffer.h
          recorders/rtp/rtpdatapacket.h
          recorders/rtp/rtpfecpacket.h
          recorders/rtp/rtcpdatapacket.h
          recorders/cetonrtsp.cpp
          recorders/iptvchannel.cpp
          recorders/iptvrecorder.cpp
          recorders/iptvsignalmonitor.cpp
          recorders/iptvstreamhandler.cpp
          recorders/streamhandler.cpp
          recorders/rtp/packetbuffer.cpp
          recorders/rtp/rtppacketbuffer.cpp
          # Support for HTTP TS streams
          recorders/httptsstreamhandler.h
          recorders/httptsstreamhandler.cpp
          # Suppport for HLS recorder
          recorders/hlsstreamhandler.h
          recorders/hlsstreamhandler.cpp
          recorders/HLS/HLSPlaylistWorker.h
          recorders/HLS/HLSReader.h
          recorders/HLS/HLSSegment.h
          recorders/HLS/HLSStream.h
          recorders/HLS/HLSStreamWorker.h
          recorders/HLS/HLSPlaylistWorker.cpp
          recorders/HLS/HLSReader.cpp
          recorders/HLS/HLSSegment.cpp
          recorders/HLS/HLSStream.cpp
          recorders/HLS/HLSStreamWorker.cpp
          # External recorder
          recorders/ExternalChannel.h
          recorders/ExternalRecChannelFetcher.h
          recorders/ExternalRecorder.h
          recorders/ExternalStreamHandler.h
          recorders/ExternalSignalMonitor.h
          recorders/ExternalChannel.cpp
          recorders/ExternalRecChannelFetcher.cpp
          recorders/ExternalRecorder.cpp
          recorders/ExternalSignalMonitor.cpp
          recorders/ExternalStreamHandler.cpp)
if(NOT WIN32)
  target_sources(
    mythtv
    PRIVATE channelscan/externrecscanner.cpp channelscan/externrecscanner.h
            recorders/v4lrecorder.cpp recorders/v4lrecorder.h)
endif()
if(TARGET PkgConfig::ALSA)
  target_link_libraries(mythtv PUBLIC PkgConfig::ALSA)
  target_sources(mythtv PRIVATE recorders/audioinputalsa.cpp
                                recorders/audioinputalsa.h)
endif()
if(TARGET mythtv_oss)
  target_link_libraries(mythtv PUBLIC mythtv_oss)
  target_sources(mythtv PRIVATE recorders/audioinputoss.cpp
                                recorders/audioinputoss.h)
endif()
if(TARGET PkgConfig::V4L2)
  target_link_libraries(mythtv PUBLIC PkgConfig::V4L2)
  target_sources(
    mythtv
    PRIVATE recorders/v4lchannel.h
            recorders/analogsignalmonitor.h
            recorders/v4l2encrecorder.h
            recorders/v4l2encstreamhandler.h
            recorders/v4l2encsignalmonitor.h
            recorders/mpegrecorder.h
            recorders/analogsignalmonitor.cpp
            recorders/mpegrecorder.cpp
            recorders/v4l2encrecorder.cpp
            recorders/v4l2encsignalmonitor.cpp
            recorders/v4l2encstreamhandler.cpp
            recorders/v4lchannel.cpp)
endif()

if(TARGET Lame::Lame)
  # Simple NuppelVideo Recorder
  if(USING_FFMPEG_THREADS)
    target_compile_definitions(mythtv PRIVATE USING_FFMPEG_THREADS)
  endif()

  if(NOT MINGW AND NOT MSVC)
    target_sources(mythtv PRIVATE recorders/NuppelVideoRecorder.cpp
                                  recorders/NuppelVideoRecorder.h)
  endif()
  target_link_libraries(mythtv PUBLIC Lame::Lame)
endif()

# Support for cable boxes that provide Firewire out
if(ENABLE_FIREWIRE)
  target_link_libraries(
    mythtv PRIVATE $<TARGET_NAME_IF_EXISTS:PkgConfig::LibAVC1394>
                   $<TARGET_NAME_IF_EXISTS:PkgConfig::LibIEC61883>)
  target_sources(
    mythtv
    PRIVATE recorders/firewirechannel.h
            recorders/firewirerecorder.h
            recorders/firewiresignalmonitor.h
            recorders/firewiredevice.h
            recorders/avcinfo.h
            recorders/avcinfo.cpp
            recorders/firewirechannel.cpp
            recorders/firewiredevice.cpp
            recorders/firewirerecorder.cpp
            recorders/firewiresignalmonitor.cpp)

  if(APPLE)
    target_compile_definitions(mythtv PRIVATE USING_FIREWIRE USING_OSX_FIREWIRE)
    target_compile_options(mythtv PRIVATE -iframework
                                          ${APPLE_AVCVIDEOSERVICES_HEADERS})
    target_sources(
      mythtv
      PRIVATE recorders/darwinfirewiredevice.h recorders/darwinavcinfo.h
              recorders/darwinavcinfo.cpp recorders/darwinfirewiredevice.cpp)
    target_link_libraries(mythtv PUBLIC ${APPLE_AVCVIDEOSERVICES_LIBRARY})
  else()
    target_compile_definitions(mythtv PRIVATE USING_LINUX_FIREWIRE)
    target_sources(
      mythtv
      PRIVATE recorders/linuxfirewiredevice.h recorders/linuxavcinfo.h
              recorders/linuxavcinfo.cpp recorders/linuxfirewiredevice.cpp)
  endif()
endif()

# Support for HDHomeRun box
if(TARGET HDHomerun::HDHomerun)
  # MythTV HDHomeRun glue
  target_link_libraries(mythtv PUBLIC HDHomerun::HDHomerun)
  target_sources(
    mythtv
    PRIVATE channelscan/hdhrchannelfetcher.h
            channelscan/hdhrchannelfetcher.cpp
            recorders/hdhrsignalmonitor.h
            recorders/hdhrchannel.h
            recorders/hdhrrecorder.h
            recorders/hdhrstreamhandler.h
            recorders/streamhandler.h
            recorders/hdhrchannel.cpp
            recorders/hdhrrecorder.cpp
            recorders/hdhrsignalmonitor.cpp
            recorders/hdhrstreamhandler.cpp
            recorders/streamhandler.cpp)
endif()

# Support for Sat>IP
if(TARGET mythtv_satip)
  target_link_libraries(mythtv PUBLIC mythtv_satip)
  target_sources(
    mythtv
    PRIVATE recorders/satiputils.h
            recorders/satipchannel.h
            recorders/satipstreamhandler.h
            recorders/satipsignalmonitor.h
            recorders/satiprtsp.h
            recorders/satiprecorder.h
            recorders/satiprtcppacket.h
            recorders/satipchannel.cpp
            recorders/satiprecorder.cpp
            recorders/satiprtcppacket.cpp
            recorders/satiprtsp.cpp
            recorders/satipsignalmonitor.cpp
            recorders/satipstreamhandler.cpp
            recorders/satiputils.cpp)
endif()

# Support for VBox
if(TARGET mythtv_vbox)
  target_link_libraries(mythtv PUBLIC mythtv_vbox)
  target_sources(
    mythtv PRIVATE recorders/vboxutils.h channelscan/vboxchannelfetcher.h
                   channelscan/vboxchannelfetcher.cpp recorders/vboxutils.cpp)
endif()

# Support for Ceton
if(TARGET mythtv_ceton)
  # MythTV Ceton glue
  target_link_libraries(mythtv PUBLIC mythtv_ceton)
  target_sources(
    mythtv
    PRIVATE recorders/cetonsignalmonitor.h
            recorders/cetonchannel.h
            recorders/cetonrecorder.h
            recorders/cetonstreamhandler.h
            recorders/streamhandler.h
            recorders/cetonchannel.cpp
            recorders/cetonrecorder.cpp
            recorders/cetonsignalmonitor.cpp
            recorders/cetonstreamhandler.cpp
            recorders/streamhandler.cpp)
endif()

# Support for Linux DVB drivers
if(TARGET mythtv_dvb)
  target_link_libraries(mythtv PUBLIC mythtv_dvb)
  target_sources(
    mythtv
    PRIVATE # Basic DVB types
            recorders/dvbtypes.h
            recorders/dvbtypes.cpp
            # Channel stuff
            recorders/dvbchannel.h
            recorders/dvbsignalmonitor.h
            recorders/dvbcam.h
            recorders/dvbchannel.cpp
            recorders/dvbsignalmonitor.cpp
            recorders/dvbcam.cpp
            # DVB Recorder
            recorders/dvbrecorder.h
            recorders/dvbstreamhandler.h
            recorders/dvbrecorder.cpp
            recorders/dvbstreamhandler.cpp
            recorders/streamhandler.h
            recorders/streamhandler.cpp
            # Misc
            recorders/dvbdev/dvbci.h
            recorders/dvbdev/dvbci.cpp)
endif()

if(TARGET mythtv_asi)
  target_link_libraries(mythtv PUBLIC mythtv_asi)
  target_sources(
    mythtv
    PRIVATE # Channel stuff
            recorders/asichannel.h
            recorders/asisignalmonitor.h
            recorders/asichannel.cpp
            recorders/asisignalmonitor.cpp
            # ASI Recorder
            recorders/asirecorder.h
            recorders/asistreamhandler.h
            recorders/asirecorder.cpp
            recorders/asistreamhandler.cpp
            recorders/streamhandler.h
            recorders/streamhandler.cpp)
endif()
