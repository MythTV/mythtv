include ( ../../settings.pro )

QT += network xml sql
contains(QT_VERSION, ^5\\.[0-9]\\..*) {
QT += widgets
}

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

POSTINC =

contains(INCLUDEPATH, /usr/include) {
  POSTINC += /usr/include
  INCLUDEPATH -= /usr/include
}
contains(INCLUDEPATH, /usr/local/include) {
  POSTINC += /usr/local/include
  INCLUDEPATH -= /usr/local/include
}

DEPENDPATH  += .
DEPENDPATH  += ../libmyth ../libmyth/audio
DEPENDPATH  += ../libmythbase
DEPENDPATH  += ../libmythdvdnav/
DEPENDPATH  += ./mpeg ./channelscan ./visualisations
DEPENDPATH  += ./recorders
DEPENDPATH  += ./recorders/dvbdev
DEPENDPATH  += ./recorders/HLS
DEPENDPATH  += ./recorders/rtp
DEPENDPATH  += ./recorders/vbitext
DEPENDPATH  += ../libmythlivemedia/BasicUsageEnvironment/include
DEPENDPATH  += ../libmythlivemedia/BasicUsageEnvironment
DEPENDPATH  += ../libmythlivemedia/groupsock/include
DEPENDPATH  += ../libmythlivemedia/groupsock
DEPENDPATH  += ../libmythlivemedia/liveMedia/include
DEPENDPATH  += ../libmythlivemedia/liveMedia
DEPENDPATH  += ../libmythlivemedia/UsageEnvironment/include
DEPENDPATH  += ../libmythlivemedia/UsageEnvironment
DEPENDPATH  += ../libmythbase ../libmythui
DEPENDPATH  += ../libmythupnp
DEPENDPATH  += ../libmythservicecontracts

INCLUDEPATH += .. ../.. # for avlib headers
INCLUDEPATH += ../../external/FFmpeg
INCLUDEPATH += $$DEPENDPATH
INCLUDEPATH += $$POSTINC

QMAKE_CXXFLAGS += $${FREETYPE_CFLAGS}
QMAKE_LFLAGS_SHLIB += $${FREETYPE_LIBS}

macx {
    # Mac OS X Frameworks
    FWKS = AGL ApplicationServices Carbon Cocoa CoreServices CoreFoundation OpenGL QuickTime IOKit
    using_quartz_video {
        FWKS += QuartzCore
    } else {
        FWKS += CoreVideo
    }

    FC = $$join(FWKS,",")

    QMAKE_CXXFLAGS += -F$${FC}
    LIBS           += -framework $$join(FWKS," -framework ")

    using_firewire:using_backend {
        QMAKE_CXXFLAGS += -F$${CONFIG_MAC_AVC}
    }
}

cygwin:QMAKE_LFLAGS_SHLIB += -Wl,--noinhibit-exec
cygwin:DEFINES += _WIN32

# Enable Valgrind, i.e. disable some timeouts
using_valgrind:DEFINES += USING_VALGRIND

# old libvbitext (Caption decoder)
#using_v4l2 {
!mingw {
    HEADERS += recorders/vbitext/cc.h
    HEADERS += recorders/vbitext/dllist.h
    HEADERS += recorders/vbitext/hamm.h
    HEADERS += recorders/vbitext/lang.h
    HEADERS += recorders/vbitext/vbi.h
    HEADERS += recorders/vbitext/vt.h
    SOURCES += recorders/vbitext/cc.cpp
    SOURCES += recorders/vbitext/vbi.c
    SOURCES += recorders/vbitext/hamm.c
    SOURCES += recorders/vbitext/lang.c
}

# mmx macros from avlib
contains( HAVE_MMX, yes ) {
    HEADERS += ../libmythbase/ffmpeg-mmx.h ../../external/FFmpeg/libavcodec/dsputil.h
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

##########################################################################
# libmythtv proper

# Headers needed by frontend & backend
HEADERS += filter.h                 format.h
HEADERS += frame.h

# LZO used by NuppelDecoder & NuppelVideoRecorder
HEADERS += lzoconf.h
HEADERS += minilzo.h
SOURCES += minilzo.cpp

# Misc. needed by backend/frontend
HEADERS += mythtvexp.h
HEADERS += recordinginfo.h
HEADERS += dbcheck.h
HEADERS += videodbcheck.h
HEADERS += tvremoteutil.h           tv.h
HEADERS += jobqueue.h
HEADERS += filtermanager.h          recordingprofile.h
HEADERS += remoteencoder.h          videosource.h
HEADERS += cardutil.h               sourceutil.h
HEADERS += videometadatautil.h
HEADERS += vbi608extractor.h
HEADERS += cc608decoder.h           cc608reader.h
HEADERS += cc708decoder.h           cc708reader.h
HEADERS += cc708window.h            subtitlereader.h
HEADERS += scheduledrecording.h
HEADERS += signalmonitorvalue.h     signalmonitorlistener.h
HEADERS += livetvchain.h            playgroup.h
HEADERS += channelsettings.h
HEADERS += previewgenerator.h       previewgeneratorqueue.h
HEADERS += transporteditor.h        listingsources.h
HEADERS += myth_imgconvert.h
HEADERS += channelgroup.h           channelgroupsettings.h
HEADERS += recordingrule.h
HEADERS += mythsystemevent.h
HEADERS += avfringbuffer.h          ThreadedFileWriter.h
HEADERS += ringbuffer.h             fileringbuffer.h
HEADERS += streamingringbuffer.h    metadataimagehelper.h
HEADERS += icringbuffer.h

SOURCES += recordinginfo.cpp
SOURCES += dbcheck.cpp
SOURCES += videodbcheck.cpp
SOURCES += tvremoteutil.cpp         tv.cpp
SOURCES += jobqueue.cpp
SOURCES += filtermanager.cpp        recordingprofile.cpp
SOURCES += remoteencoder.cpp        videosource.cpp
SOURCES += cardutil.cpp             sourceutil.cpp
SOURCES += videometadatautil.cpp
SOURCES += vbi608extractor.cpp
SOURCES += cc608decoder.cpp         cc608reader.cpp
SOURCES += cc708decoder.cpp         cc708reader.cpp
SOURCES += cc708window.cpp          subtitlereader.cpp
SOURCES += scheduledrecording.cpp
SOURCES += signalmonitorvalue.cpp
SOURCES += livetvchain.cpp          playgroup.cpp
SOURCES += channelsettings.cpp
SOURCES += previewgenerator.cpp     previewgeneratorqueue.cpp
SOURCES += transporteditor.cpp
SOURCES += channelgroup.cpp         channelgroupsettings.cpp
SOURCES += myth_imgconvert.cpp
SOURCES += recordingrule.cpp
SOURCES += mythsystemevent.cpp
SOURCES += avfringbuffer.cpp        ThreadedFileWriter.cpp
SOURCES += ringbuffer.cpp           fileringBuffer.cpp
SOURCES += streamingringbuffer.cpp  metadataimagehelper.cpp
SOURCES += icringbuffer.cpp

# DiSEqC
HEADERS += diseqc.h                 diseqcsettings.h
SOURCES += diseqc.cpp               diseqcsettings.cpp

# Listings downloading classes
HEADERS += datadirect.h
SOURCES += datadirect.cpp

# File/FIFO Writer classes
HEADERS += filewriterbase.h         avformatwriter.h
HEADERS += fifowriter.h
SOURCES += filewriterbase.cpp       avformatwriter.cpp
SOURCES += fifowriter.cpp

# Teletext stuff
HEADERS += teletextdecoder.h        teletextreader.h   vbilut.h
SOURCES += teletextdecoder.cpp      teletextreader.cpp vbilut.cpp

# MPEG parsing stuff
HEADERS += mpeg/tspacket.h          mpeg/pespacket.h
HEADERS += mpeg/mpegtables.h        mpeg/atsctables.h
HEADERS += mpeg/dvbtables.h         mpeg/premieretables.h
HEADERS += mpeg/sctetables.h
HEADERS += mpeg/mpegstreamdata.h    mpeg/atscstreamdata.h
HEADERS += mpeg/dvbstreamdata.h     mpeg/scanstreamdata.h
HEADERS += mpeg/mpegdescriptors.h   mpeg/atscdescriptors.h
HEADERS += mpeg/sctedescriptors.h   mpeg/dvbdescriptors.h
HEADERS += mpeg/splicedescriptors.h
HEADERS += mpeg/dishdescriptors.h   mpeg/premieredescriptors.h
HEADERS += mpeg/atsc_huffman.h
HEADERS += mpeg/freesat_huffman.h   mpeg/freesat_tables.h
HEADERS += mpeg/iso6937tables.h
HEADERS += mpeg/tsstats.h           mpeg/streamlisteners.h
HEADERS += mpeg/H264Parser.h

SOURCES += mpeg/tspacket.cpp        mpeg/pespacket.cpp
SOURCES += mpeg/mpegtables.cpp      mpeg/atsctables.cpp
SOURCES += mpeg/dvbtables.cpp       mpeg/premieretables.cpp
SOURCES += mpeg/sctetables.cpp
SOURCES += mpeg/mpegstreamdata.cpp  mpeg/atscstreamdata.cpp
SOURCES += mpeg/dvbstreamdata.cpp   mpeg/scanstreamdata.cpp
SOURCES += mpeg/mpegdescriptors.cpp mpeg/atscdescriptors.cpp
SOURCES += mpeg/dvbdescriptors.cpp  mpeg/sctedescriptors.cpp
SOURCES += mpeg/splicedescriptors.cpp
SOURCES += mpeg/dishdescriptors.cpp mpeg/premieredescriptors.cpp
SOURCES += mpeg/atsc_huffman.cpp
SOURCES += mpeg/freesat_huffman.cpp
SOURCES += mpeg/iso6937tables.cpp
SOURCES += mpeg/H264Parser.cpp

# Channels, and the multiplexes that transmit them
HEADERS += frequencies.h            frequencytables.h
SOURCES += frequencies.cpp          frequencytables.cpp

HEADERS += channelutil.h            channelinfo.h
HEADERS += iptvtuningdata.h
SOURCES += channelutil.cpp          channelinfo.cpp

HEADERS += dtvmultiplex.h
HEADERS += dtvconfparser.h          dtvconfparserhelpers.h
SOURCES += dtvmultiplex.cpp
SOURCES += dtvconfparser.cpp        dtvconfparserhelpers.cpp

HEADERS += channelscan/scaninfo.h   channelscan/channelimporter.h
HEADERS += channelscan/iptvchannelfetcher.h
SOURCES += channelscan/scaninfo.cpp channelscan/channelimporter.cpp
SOURCES += channelscan/iptvchannelfetcher.cpp

# subtitles: srt
HEADERS += srtwriter.h
SOURCES += srtwriter.cpp

inc.path = $${PREFIX}/include/mythtv/
inc.files  = playgroup.h
inc.files += mythtvexp.h            metadataimagehelper.h

INSTALLS += inc

#DVD stuff
DEPENDPATH  += ../libmythdvdnav/
INCLUDEPATH += ../libmythdvdnav
POST_TARGETDEPS += ../libmythdvdnav/libmythdvdnav-$${MYTH_LIB_EXT}
HEADERS += DVD/dvdringbuffer.h
SOURCES += DVD/dvdringbuffer.cpp
using_frontend {
    HEADERS += DVD/mythdvdplayer.h
    SOURCES += DVD/mythdvdplayer.cpp
    HEADERS += DVD/avformatdecoderdvd.h
    SOURCES += DVD/avformatdecoderdvd.cpp
}
LIBS += -L../libmythdvdnav
LIBS += -lmythdvdnav-$$LIBVERSION

#Bluray stuff
DEPENDPATH   += ../../external/libmythbluray/
INCLUDEPATH  += ../../external/libmythbluray/
POST_TARGETDEPS += ../../external/libmythbluray/libmythbluray-$${MYTH_LIB_EXT}
HEADERS += Bluray/bdringbuffer.h
SOURCES += Bluray/bdringbuffer.cpp
using_frontend {
    HEADERS += Bluray/mythbdplayer.h
    SOURCES += Bluray/mythbdplayer.cpp
    HEADERS += Bluray/avformatdecoderbd.h
    SOURCES += Bluray/avformatdecoderbd.cpp
    HEADERS += Bluray/bdoverlayscreen.h
    SOURCES += Bluray/bdoverlayscreen.cpp
}
LIBS += -L../../external/libmythbluray
LIBS += -lmythbluray-$$LIBVERSION

#HLS stuff
HEADERS += HLS/httplivestream.h
SOURCES += HLS/httplivestream.cpp
HEADERS += HLS/httplivestreambuffer.h
SOURCES += HLS/httplivestreambuffer.cpp
using_libcrypto:DEFINES += USING_LIBCRYPTO
using_libcrypto:LIBS    += -lcrypto


using_frontend {
    # Recording profile stuff
    HEADERS += profilegroup.h
    SOURCES += profilegroup.cpp

    # Video playback
    HEADERS += tv_play.h                mythplayer.h
    HEADERS += audioplayer.h
    HEADERS += mythccextractorplayer.h  teletextextractorreader.h
    HEADERS += playercontext.h
    HEADERS += tv_play_win.h            deletemap.h
    HEADERS += mythcommflagplayer.h     commbreakmap.h
    HEADERS += mythiowrapper.h          tvbrowsehelper.h
    HEADERS += netstream.h
    SOURCES += tv_play.cpp              mythplayer.cpp
    SOURCES += audioplayer.cpp
    SOURCES += mythccextractorplayer.cpp teletextextractorreader.cpp
    SOURCES += playercontext.cpp
    SOURCES += tv_play_win.cpp          deletemap.cpp
    SOURCES += mythcommflagplayer.cpp   commbreakmap.cpp
    SOURCES += mythiowrapper.cpp        tvbrowsehelper.cpp
    SOURCES += netstream.cpp

    # Text subtitle parser
    HEADERS += textsubtitleparser.h     xine_demux_sputext.h
    SOURCES += textsubtitleparser.cpp   xine_demux_sputext.cpp

    # A/V decoders
    HEADERS += decoderbase.h
    HEADERS += nuppeldecoder.h          avformatdecoder.h
    HEADERS += privatedecoder.h
    SOURCES += decoderbase.cpp
    SOURCES += nuppeldecoder.cpp        avformatdecoder.cpp
    SOURCES += privatedecoder.cpp

    using_crystalhd {
        DEFINES += USING_CRYSTALHD
        HEADERS += privatedecoder_crystalhd.h
        SOURCES += privatedecoder_crystalhd.cpp
        LIBS += -lcrystalhd
    }

    using_libass {
        DEFINES += USING_LIBASS
        LIBS    += -lass
    }

    macx {
        HEADERS += privatedecoder_vda.h privatedecoder_vda_defs.h
        SOURCES += privatedecoder_vda.cpp
    }

    # On screen display (video output overlay)
    HEADERS += osd.h                    teletextscreen.h
    HEADERS += subtitlescreen.h         interactivescreen.h
    SOURCES += osd.cpp                  teletextscreen.cpp
    SOURCES += subtitlescreen.cpp       interactivescreen.cpp

    # Video output
    HEADERS += videooutbase.h           videoout_null.h
    HEADERS += videobuffers.h           vsync.h
    HEADERS += jitterometer.h           yuv2rgb.h
    HEADERS += videodisplayprofile.h    mythcodecid.h
    HEADERS += videoouttypes.h          util-osd.h
    HEADERS += videooutwindow.h         videocolourspace.h
    HEADERS += visualisations/videovisual.h
    HEADERS += visualisations/videovisualdefs.h
    SOURCES += videooutbase.cpp         videoout_null.cpp
    SOURCES += videobuffers.cpp         vsync.cpp
    SOURCES += jitterometer.cpp         yuv2rgb.cpp
    SOURCES += videodisplayprofile.cpp  mythcodecid.cpp
    SOURCES += videooutwindow.cpp       util-osd.cpp
    SOURCES += videocolourspace.cpp
    SOURCES += visualisations/videovisual.cpp

   using_opengl | using_vdpau {
        # Goom
        HEADERS += goom/filters.h goom/goomconfig.h goom/goom_core.h goom/graphic.h
        HEADERS += goom/ifs.h goom/lines.h goom/drawmethods.h
        HEADERS += goom/mmx.h goom/mathtools.h goom/tentacle3d.h goom/v3d.h
        HEADERS += videovisualgoom.h
        SOURCES += goom/filters.c goom/goom_core.c goom/graphic.c goom/tentacle3d.c
        SOURCES += goom/ifs.c goom/ifs_display.c goom/lines.c goom/surf3d.c
        SOURCES += goom/zoom_filter_mmx.c goom/zoom_filter_xmmx.c
        SOURCES += videovisualgoom.cpp
    }

    using_libfftw3 {
        DEFINES += FFTW3_SUPPORT
        HEADERS += visualisations/videovisualspectrum.h
        SOURCES += visualisations/videovisualspectrum.cpp
        using_opengl: HEADERS += videovisualcircles.h
        using_opengl: SOURCES += videovisualcircles.cpp
    }

    using_quartz_video: DEFINES += USING_QUARTZ_VIDEO
    using_quartz_video: HEADERS += videoout_quartz.h
    using_quartz_video: SOURCES += videoout_quartz.cpp

    using_x11:DEFINES += USING_X11

    using_xv:HEADERS += videoout_xv.h   util-xv.h   osdchromakey.h
    using_xv:SOURCES += videoout_xv.cpp util-xv.cpp osdchromakey.cpp

    using_xv:DEFINES += USING_XV

    using_vdpau {
        DEFINES += USING_VDPAU
        HEADERS += videoout_vdpau.h   videoout_nullvdpau.h
        SOURCES += videoout_vdpau.cpp videoout_nullvdpau.cpp
        LIBS += -lvdpau
    }

    using_opengl {
        CONFIG += opengl
        DEFINES += USING_OPENGL
        HEADERS += util-opengl.h
        SOURCES += util-opengl.cpp
        QT += opengl
    }

    using_opengl_video:DEFINES += USING_OPENGL_VIDEO
    using_opengl_video:HEADERS += openglvideo.h   videoout_opengl.h
    using_opengl_video:SOURCES += openglvideo.cpp videoout_opengl.cpp

    using_vaapi {
        DEFINES += USING_VAAPI
        HEADERS += vaapicontext.h   videoout_nullvaapi.h
        SOURCES += vaapicontext.cpp videoout_nullvaapi.cpp
        LIBS    += -lva -lva-x11 -lva-glx
        using_opengl_video:HEADERS += videoout_openglvaapi.h
        using_opengl_video:SOURCES += videoout_openglvaapi.cpp
        using_opengl_video:DEFINES += USING_GLVAAPI
    }

    # Misc. frontend
    HEADERS += DetectLetterbox.h
    SOURCES += DetectLetterbox.cpp

    using_libdns_sd {
        !macx: LIBS += -ldns_sd
        using_libcrypto {
            HEADERS += AirPlay/mythairplayserver.h
            SOURCES += AirPlay/mythairplayserver.cpp
            HEADERS += AirPlay/mythraopdevice.h   AirPlay/mythraopconnection.h
            SOURCES += AirPlay/mythraopdevice.cpp AirPlay/mythraopconnection.cpp
            DEFINES += USING_AIRPLAY
        }
    }

    using_mheg {
        # DSMCC stuff
        HEADERS += dsmcc.h                  dsmcccache.h
        HEADERS += dsmccbiop.h              dsmccobjcarousel.h
        HEADERS += dsmccreceiver.h
        SOURCES += dsmcc.cpp                dsmcccache.cpp
        SOURCES += dsmccbiop.cpp            dsmccobjcarousel.cpp

         # MHEG interaction channel
        HEADERS += mhegic.h
        SOURCES += mhegic.cpp

        # MHEG/MHI stuff
        HEADERS += interactivetv.h          mhi.h
        SOURCES += interactivetv.cpp        mhi.cpp
        DEFINES += USING_MHEG
    }

    # C stuff
    HEADERS += blend.h
    SOURCES += blend.c

    DEFINES += USING_FRONTEND
}

using_backend {
    # Channel stuff
    HEADERS += recorders/channelbase.h
    HEADERS += recorders/dtvchannel.h
    HEADERS += recorders/signalmonitor.h
    HEADERS += recorders/dtvsignalmonitor.h
    HEADERS += recorders/scriptsignalmonitor.h
    SOURCES += recorders/channelbase.cpp
    SOURCES += recorders/dtvchannel.cpp
    SOURCES += recorders/signalmonitor.cpp
    SOURCES += recorders/dtvsignalmonitor.cpp

    HEADERS += inputinfo.h                 inputgroupmap.h
    SOURCES += inputinfo.cpp               inputgroupmap.cpp

    # Channel scanner stuff
    HEADERS += scanwizard.h
    SOURCES += scanwizard.cpp

    HEADERS += channelscan/channelscan_sm.h
    HEADERS += channelscan/channelscanner.h
    HEADERS += channelscan/channelscanner_gui.h
    HEADERS += channelscan/channelscanner_gui_scan_pane.h
    HEADERS += channelscan/channelscanner_cli.h
    HEADERS += channelscan/frequencytablesetting.h
    HEADERS += channelscan/inputselectorsetting.h
    HEADERS += channelscan/loglist.h
    HEADERS += channelscan/channelscanmiscsettings.h
    HEADERS += channelscan/modulationsetting.h
    HEADERS += channelscan/multiplexsetting.h
    HEADERS += channelscan/paneanalog.h
    HEADERS += channelscan/paneatsc.h
    HEADERS += channelscan/panedvbc.h
    HEADERS += channelscan/panedvbs.h
    HEADERS += channelscan/panedvbs2.h
    HEADERS += channelscan/panedvbt.h
    HEADERS += channelscan/panedvbutilsimport.h
    HEADERS += channelscan/panesingle.h
    HEADERS += channelscan/scanmonitor.h
    HEADERS += channelscan/scanwizardconfig.h

    SOURCES += channelscan/channelscan_sm.cpp
    SOURCES += channelscan/channelscanner.cpp
    SOURCES += channelscan/channelscanner_gui.cpp
    SOURCES += channelscan/channelscanner_gui_scan_pane.cpp
    SOURCES += channelscan/channelscanner_cli.cpp
    SOURCES += channelscan/frequencytablesetting.cpp
    SOURCES += channelscan/inputselectorsetting.cpp
    SOURCES += channelscan/loglist.cpp
    SOURCES += channelscan/multiplexsetting.cpp
    SOURCES += channelscan/paneanalog.cpp
    SOURCES += channelscan/scanmonitor.cpp
    SOURCES += channelscan/scanwizardconfig.cpp

    # EIT stuff
    HEADERS += eithelper.h                 eitscanner.h
    HEADERS += eitfixup.h                  eitcache.h
    SOURCES += eithelper.cpp               eitscanner.cpp
    SOURCES += eitfixup.cpp                eitcache.cpp

    # non-EIT EPG stuff
    HEADERS += programdata.h
    SOURCES += programdata.cpp

    # TVRec stuff
    HEADERS += tv_rec.h                    recordingquality.h
    SOURCES += tv_rec.cpp                  recordingquality.cpp

    # Recorder base and util classes
    HEADERS += recorders/recorderbase.h
    HEADERS += recorders/DeviceReadBuffer.h
    HEADERS += recorders/dtvrecorder.h
    SOURCES += recorders/recorderbase.cpp
    SOURCES += recorders/DeviceReadBuffer.cpp
    SOURCES += recorders/dtvrecorder.cpp

    # Import recorder
    HEADERS += recorders/importrecorder.h
    SOURCES += recorders/importrecorder.cpp

    # Simple NuppelVideo Recorder
    using_ffmpeg_threads:DEFINES += USING_FFMPEG_THREADS
    !mingw:HEADERS += recorders/NuppelVideoRecorder.h
    HEADERS += recorders/RTjpegN.h
    HEADERS += recorders/audioinput.h
    HEADERS += recorders/go7007_myth.h

    !mingw:SOURCES += recorders/NuppelVideoRecorder.cpp
    SOURCES += recorders/RTjpegN.cpp
    SOURCES += recorders/audioinput.cpp
    using_alsa {
        HEADERS += recorders/audioinputalsa.h
        SOURCES += recorders/audioinputalsa.cpp
        DEFINES += USING_ALSA
    }
    using_oss {
        HEADERS += recorders/audioinputoss.h
        SOURCES += recorders/audioinputoss.cpp
        DEFINES += USING_OSS
    }

    # Support for Video4Linux devices
    !mingw {
        HEADERS += recorders/v4lrecorder.h
        SOURCES += recorders/v4lrecorder.cpp
    }
    using_v4l2 {
        HEADERS += recorders/v4lchannel.h
        HEADERS += recorders/analogsignalmonitor.h
        SOURCES += recorders/v4lchannel.cpp
        SOURCES += recorders/analogsignalmonitor.cpp

        DEFINES += USING_V4L2
    }

    # Support for cable boxes that provide Firewire out
    using_firewire {
        HEADERS += recorders/firewirechannel.h
        HEADERS += recorders/firewirerecorder.h
        HEADERS += recorders/firewiresignalmonitor.h
        HEADERS += recorders/firewiredevice.h
        HEADERS += recorders/avcinfo.h
        SOURCES += recorders/firewirechannel.cpp
        SOURCES += recorders/firewirerecorder.cpp
        SOURCES += recorders/firewiresignalmonitor.cpp
        SOURCES += recorders/firewiredevice.cpp
        SOURCES += recorders/avcinfo.cpp

        macx {
            HEADERS += recorders/darwinfirewiredevice.h
            HEADERS += recorders/darwinavcinfo.h
            SOURCES += recorders/darwinfirewiredevice.cpp
            SOURCES += recorders/darwinavcinfo.cpp
            DEFINES += USING_OSX_FIREWIRE
        }

        !macx {
            HEADERS += recorders/linuxfirewiredevice.h
            HEADERS += recorders/linuxavcinfo.h
            SOURCES += recorders/linuxfirewiredevice.cpp
            SOURCES += recorders/linuxavcinfo.cpp
            DEFINES += USING_LINUX_FIREWIRE
        }

        DEFINES += USING_FIREWIRE
    }

    # Support for RTP/UDP streams
    HEADERS += recorders/cetonrtsp.h
    HEADERS += recorders/iptvchannel.h
    HEADERS += recorders/iptvrecorder.h
    HEADERS += recorders/iptvsignalmonitor.h
    HEADERS += recorders/iptvstreamhandler.h
    HEADERS *= recorders/streamhandler.h

    HEADERS += recorders/rtp/udppacket.h
    HEADERS += recorders/rtp/udppacketbuffer.h
    HEADERS += recorders/rtp/packetbuffer.h
    HEADERS += recorders/rtp/rtppacketbuffer.h
    HEADERS += recorders/rtp/rtpdatapacket.h
    HEADERS += recorders/rtp/rtpfecpacket.h

    SOURCES += recorders/cetonrtsp.cpp
    SOURCES += recorders/iptvchannel.cpp
    SOURCES += recorders/iptvrecorder.cpp
    SOURCES += recorders/iptvsignalmonitor.cpp
    SOURCES += recorders/iptvstreamhandler.cpp
    SOURCES *= recorders/streamhandler.cpp

    SOURCES += recorders/rtp/packetbuffer.cpp
    SOURCES += recorders/rtp/rtppacketbuffer.cpp

    DEFINES += USING_IPTV

    # Support for HDHomeRun box
    using_hdhomerun {
        # MythTV HDHomeRun glue
        INCLUDEPATH += ../../external/libhdhomerun
        DEPENDPATH += ../../external/libhdhomerun

        HEADERS += recorders/hdhrsignalmonitor.h
        HEADERS += recorders/hdhrchannel.h
        HEADERS += recorders/hdhrrecorder.h
        HEADERS += recorders/hdhrstreamhandler.h

        SOURCES += recorders/hdhrsignalmonitor.cpp
        SOURCES += recorders/hdhrchannel.cpp
        SOURCES += recorders/hdhrrecorder.cpp
        SOURCES += recorders/hdhrstreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        DEFINES += USING_HDHOMERUN
    }

    # Support for Ceton
    using_ceton {
        # MythTV Ceton glue
        HEADERS += recorders/cetonsignalmonitor.h
        HEADERS += recorders/cetonchannel.h
        HEADERS += recorders/cetonrecorder.h
        HEADERS += recorders/cetonstreamhandler.h

        SOURCES += recorders/cetonsignalmonitor.cpp
        SOURCES += recorders/cetonchannel.cpp
        SOURCES += recorders/cetonrecorder.cpp
        SOURCES += recorders/cetonstreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        DEFINES += USING_CETON
    }

    # Support for PVR-150/250/350/500, etc. on Linux
    using_ivtv:HEADERS *= recorders/mpegrecorder.h
    using_ivtv:SOURCES *= recorders/mpegrecorder.cpp
    using_ivtv:DEFINES += USING_IVTV

    # Support for HD-PVR on Linux
    using_hdpvr:HEADERS *= recorders/mpegrecorder.h
    using_hdpvr:SOURCES *= recorders/mpegrecorder.cpp
    using_hdpvr:DEFINES += USING_HDPVR

    # Support for Linux DVB drivers
    using_dvb {
        # Basic DVB types
        HEADERS += recorders/dvbtypes.h
        SOURCES += recorders/dvbtypes.cpp

        # Channel stuff
        HEADERS += recorders/dvbchannel.h
        HEADERS += recorders/dvbsignalmonitor.h
        HEADERS += recorders/dvbcam.h
        SOURCES += recorders/dvbchannel.cpp
        SOURCES += recorders/dvbsignalmonitor.cpp
        SOURCES += recorders/dvbcam.cpp

        # DVB Recorder
        HEADERS += recorders/dvbrecorder.h
        HEADERS += recorders/dvbstreamhandler.h
        SOURCES += recorders/dvbrecorder.cpp
        SOURCES += recorders/dvbstreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        # Misc
        HEADERS += recorders/dvbdev/dvbci.h
        SOURCES += recorders/dvbdev/dvbci.cpp

        DEFINES += USING_DVB
    }

    using_asi {
        # Channel stuff
        HEADERS += recorders/asichannel.h
        HEADERS += recorders/asisignalmonitor.h
        SOURCES += recorders/asichannel.cpp
        SOURCES += recorders/asisignalmonitor.cpp

        # ASI Recorder
        HEADERS += recorders/asirecorder.h
        HEADERS += recorders/asistreamhandler.h
        SOURCES += recorders/asirecorder.cpp
        SOURCES += recorders/asistreamhandler.cpp

        HEADERS *= recorders/streamhandler.h
        SOURCES *= recorders/streamhandler.cpp

        DEFINES += USING_ASI
    }

    DEFINES += USING_BACKEND
}

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

mingw {
    DEFINES += USING_MINGW

    HEADERS += videoout_d3d.h
    SOURCES += videoout_d3d.cpp

    using_dxva2: DEFINES += USING_DXVA2
    using_dxva2: HEADERS += dxva2decoder.h
    using_dxva2: SOURCES += dxva2decoder.cpp

    LIBS += -lws2_32
}

# Dependencies and required libraries
# Have them at the end in order to properly resolve on mingw platform
# where the order is of significance
LIBS += -L../libmyth
LIBS += -L../../external/FFmpeg/libavutil
LIBS += -L../../external/FFmpeg/libavcodec
LIBS += -L../../external/FFmpeg/libavformat
LIBS += -L../../external/FFmpeg/libswscale
LIBS += -L../libmythui -L../libmythupnp
LIBS += -L../libmythbase
LIBS += -L../libmythservicecontracts
LIBS += -lmyth-$$LIBVERSION
LIBS += -lmythswscale
LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lmythui-$$LIBVERSION       -lmythupnp-$$LIBVERSION
LIBS += -lmythbase-$$LIBVERSION
LIBS += -lmythservicecontracts-$$LIBVERSION
using_mheg: LIBS += -L../libmythfreemheg -lmythfreemheg-$$LIBVERSION
using_live: LIBS += -L../libmythlivemedia -lmythlivemedia-$$LIBVERSION
using_hdhomerun: LIBS += -L../../external/libhdhomerun -lmythhdhomerun-$$LIBVERSION
using_backend: LIBS += -lmp3lame
LIBS += $$EXTRA_LIBS $$QMAKE_LIBS_DYNLOAD

POST_TARGETDEPS += ../libmyth/libmyth-$${MYTH_SHLIB_EXT}
POST_TARGETDEPS += ../../external/FFmpeg/libavutil/$$avLibName(avutil)
POST_TARGETDEPS += ../../external/FFmpeg/libavcodec/$$avLibName(avcodec)
POST_TARGETDEPS += ../../external/FFmpeg/libavformat/$$avLibName(avformat)
POST_TARGETDEPS += ../../external/FFmpeg/libswscale/$$avLibName(swscale)
using_mheg: POST_TARGETDEPS += ../libmythfreemheg/libmythfreemheg-$${MYTH_SHLIB_EXT}
using_live: POST_TARGETDEPS += ../libmythlivemedia/libmythlivemedia-$${MYTH_SHLIB_EXT}
using_hdhomerun: POST_TARGETDEPS += ../../external/libhdhomerun/libmythhdhomerun-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}

include ( ../libs-targetfix.pro )

LIBS += $$LATE_LIBS
DEFINES += MTV_API
