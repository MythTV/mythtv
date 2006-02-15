include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

INCLUDEPATH += ../.. ..
INCLUDEPATH += ../libmyth ../libavcodec ../libavutil ../libmythmpeg2
INCLUDEPATH += ./dvbdev ./mpeg
DEPENDPATH  += ../libmyth ../libavcodec ../libavformat ../libavutil
DEPENDPATH  += ../libmythmpeg2
DEPENDPATH  += ./dvbdev ./mpeg

LIBS += -L../libmyth -L../libavutil -L../libavcodec -L../libavformat -L../libmythmpeg2
LIBS += -lmyth-$${LIBVERSION} -lmythavutil-$${LIBVERSION} \
        -lmythavcodec-$${LIBVERSION} \
        -lmythavformat-$${LIBVERSION} -lmythmpeg2-$${LIBVERSION} \
        $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libavutil/libmythavutil-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libmythmpeg2/libmythmpeg2-$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}

DEFINES += _LARGEFILE_SOURCE
QMAKE_CXXFLAGS_RELEASE += $${FREETYPE_CFLAGS}
QMAKE_CXXFLAGS_DEBUG += $${FREETYPE_CFLAGS}
QMAKE_LFLAGS_SHLIB += $${FREETYPE_LIBS}

macx {
    # Mac OS X Frameworks
    FWKS = AGL ApplicationServices Carbon Cocoa OpenGL QuickTime
    PFWKS = DVD

    # The following trick shortens the command line, but depends on
    # the shell expanding Csh-style braces. Luckily, Bash and Zsh do.
    FC = $$join(FWKS,",","{","}")
    PFC = $$join(PFWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    QMAKE_CXXFLAGS += -F/System/Library/PrivateFrameworks/$${PFC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")
    LIBS           += -F/System/Library/PrivateFrameworks
    LIBS           += -framework $$join(PFWKS," -framework ")

    using_mac_accel {
        # accel_utils uses Objective-C++, activated by .mm suffix
        QMAKE_EXT_CPP += .mm
    }

    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC9000000

    DEFINES += __BUILD_ALTIVEC_ASM__
    SOURCES += memcpyaltivec.c
    HEADERS += memcpy.h
}

# Enable Linux Open Sound System support
using_oss:DEFINES += USING_OSS
# Enable Valgrind, i.e. disable some timeouts
using_valgrind:DEFINES += USING_VALGRIND

# old libvbitext (Caption decoder)
!win32 {
    HEADERS += vbitext/cc.h vbitext/dllist.h vbitext/hamm.h vbitext/lang.h 
    HEADERS += vbitext/vbi.h vbitext/vt.h
    SOURCES += vbitext/cc.cpp vbitext/vbi.c vbitext/hamm.c vbitext/lang.c
}

# mmx macros from avlib
contains( TARGET_MMX, yes ) {
    HEADERS += ../../libs/libavcodec/i386/mmx.h ../../libs/libavcodec/dsputil.h
}

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

##########################################################################
# libmythtv proper

# Headers needed by frontend & backend
HEADERS += filter.h                 format.h
HEADERS += frame.h

# LZO / RTjpegN, used by NuppelDecoder & NuppelVideoRecorder
HEADERS += lzoconf.h
HEADERS += minilzo.h                RTjpegN.h
SOURCES += minilzo.cpp              RTjpegN.cpp

# Misc. needed by backend/frontend
HEADERS += programinfo.h            proglist.h
HEADERS += RingBuffer.h             DVDRingBuffer.h
HEADERS += ThreadedFileWriter.h     previouslist.h
HEADERS += dbcheck.h                
HEADERS += remoteutil.h             tv.h
HEADERS += recordingtypes.h         jobqueue.h
HEADERS += filtermanager.h          recordingprofile.h
HEADERS += remoteencoder.h          videosource.h
HEADERS += ccdecoder.h
HEADERS += sr_dialog.h              sr_root.h
HEADERS += sr_items.h               scheduledrecording.h
HEADERS += signalmonitorvalue.h     viewschdiff.h
HEADERS += mpeg/tspacket.h          mpeg/pespacket.h
HEADERS += mpeg/iso639.h
HEADERS += livetvchain.h
HEADERS += playgroup.h              progdetails.h

SOURCES += programinfo.cpp          proglist.cpp
SOURCES += RingBuffer.cpp           DVDRingBuffer.cpp
SOURCES += ThreadedFileWriter.cpp   previouslist.cpp
SOURCES += dbcheck.cpp              
SOURCES += remoteutil.cpp           tv.cpp
SOURCES += recordingtypes.cpp       jobqueue.cpp
SOURCES += filtermanager.cpp        recordingprofile.cpp
SOURCES += remoteencoder.cpp        videosource.cpp
SOURCES += ccdecoder.cpp
SOURCES += sr_dialog.cpp            sr_root.cpp
SOURCES += sr_items.cpp             scheduledrecording.cpp
SOURCES += signalmonitorvalue.cpp
SOURCES += mpeg/tspacket.cpp        mpeg/pespacket.cpp
SOURCES += mpeg/iso639.cpp
SOURCES += viewschdiff.cpp
SOURCES += livetvchain.cpp
SOURCES += playgroup.cpp
SOURCES += progdetails.cpp

using_frontend {
    # Recording profile stuff
    HEADERS += profilegroup.h
    SOURCES += profilegroup.cpp

    # XBox LED control
    HEADERS += xbox.h
    SOURCES += xbox.cpp

    # Video playback
    HEADERS += tv_play.h                NuppelVideoPlayer.h
    SOURCES += tv_play.cpp              NuppelVideoPlayer.cpp

    # A/V decoders
    HEADERS += decoderbase.h
    HEADERS += nuppeldecoder.h          avformatdecoder.h
    SOURCES += decoderbase.cpp
    SOURCES += nuppeldecoder.cpp        avformatdecoder.cpp 

    using_ivtv:HEADERS += ivtvdecoder.h
    using_ivtv:SOURCES += ivtvdecoder.cpp

    # On screen display (video output overlay)
    HEADERS += osd.h                    osdtypes.h
    HEADERS += osdsurface.h             osdlistbtntype.h
    HEADERS += udpnotify.h 
    SOURCES += osd.cpp                  osdtypes.cpp
    SOURCES += osdsurface.cpp           osdlistbtntype.cpp
    SOURCES += udpnotify.cpp 

    # Video output
    HEADERS += videooutbase.h           videoout_null.h
    HEADERS += videobuffers.h           vsync.h
    HEADERS += jitterometer.h           yuv2rgb.h
    SOURCES += videooutbase.cpp         videoout_null.cpp
    SOURCES += videobuffers.cpp         vsync.cpp
    SOURCES += jitterometer.cpp         yuv2rgb.cpp

    using_opengl_vsync:CONFIG +=  opengl
    using_opengl_vsync:DEFINES += USING_OPENGL_VSYNC

    macx:HEADERS +=               videoout_quartz.h
    macx:SOURCES +=               videoout_quartz.cpp

    using_mac_accel:HEADERS +=    videoout_accel_utils.h
    using_mac_accel:HEADERS +=    videoout_accel_private.h
    using_mac_accel:SOURCES +=    videoout_accel_utils.mm

    using_directfb:HEADERS +=     videoout_directfb.h
    using_directfb:SOURCES +=     videoout_directfb.cpp
    using_directfb:DEFINES +=     USING_DIRECTFB

    using_directx:HEADERS +=      videoout_dx.h
    using_directx:SOURCES +=      videoout_dx.cpp

    using_ivtv:HEADERS +=         videoout_ivtv.h
    using_ivtv:SOURCES +=         videoout_ivtv.cpp

    using_xv:HEADERS += videoout_xv.h   XvMCSurfaceTypes.h   osdxvmc.h
    using_xv:SOURCES += videoout_xv.cpp XvMCSurfaceTypes.cpp osdxvmc.cpp
    using_xv:DEFINES += USING_XV

    using_xvmc:DEFINES += USING_XVMC
    using_xvmcw:DEFINES += USING_XVMCW
    using_xvmc_vld:DEFINES += USING_XVMC_VLD

    # Misc. frontend
    HEADERS += guidegrid.h              infostructs.h
    HEADERS += progfind.h               ttfont.h
    SOURCES += guidegrid.cpp            infostructs.cpp
    SOURCES += progfind.cpp             ttfont.cpp

    # Teletext stuff
    HEADERS += vbilut.h
    SOURCES += vbilut.cpp

    # C stuff
    HEADERS += blend.h
    SOURCES += blend.c

    DEFINES += USING_FRONTEND
}

using_backend {
    # Channel stuff
    HEADERS += channelbase.h               channelutil.h
    HEADERS += signalmonitor.h             dtvsignalmonitor.h
    SOURCES += channelbase.cpp             channelutil.cpp
    SOURCES += signalmonitor.cpp           dtvsignalmonitor.cpp

    # Channel scanner stuff
    HEADERS += scanwizard.h                scanwizardhelpers.h
    HEADERS += siscan.h                    frequencytables.h
    SOURCES += scanwizard.cpp              scanwizardhelpers.cpp
    SOURCES += siscan.cpp                  frequencytables.cpp

    # TVRec & Recorder base classes
    HEADERS += tv_rec.h
    HEADERS += recorderbase.h              DeviceReadBuffer.h
    HEADERS += dtvrecorder.h               dummydtvrecorder.h
    SOURCES += tv_rec.cpp
    SOURCES += recorderbase.cpp            DeviceReadBuffer.cpp
    SOURCES += dtvrecorder.cpp             dummydtvrecorder.cpp

    # MPEG parsing stuff
    HEADERS += mpeg/mpegtables.h           mpeg/atsctables.h
    HEADERS += mpeg/dvbtables.h
    HEADERS += mpeg/mpegstreamdata.h       mpeg/atscstreamdata.h
    HEADERS += mpeg/dvbstreamdata.h        mpeg/scanstreamdata.h
    HEADERS += mpeg/mpegdescriptors.h      mpeg/atscdescriptors.h
    HEADERS += mpeg/dvbdescriptors.h
    HEADERS += mpeg/tsstats.h 
    SOURCES += mpeg/mpegtables.cpp         mpeg/atsctables.cpp
    SOURCES += mpeg/dvbtables.cpp
    SOURCES += mpeg/mpegstreamdata.cpp     mpeg/atscstreamdata.cpp
    SOURCES += mpeg/dvbstreamdata.cpp      mpeg/scanstreamdata.cpp
    SOURCES += mpeg/mpegdescriptors.cpp    mpeg/atscdescriptors.cpp
    SOURCES += mpeg/dvbdescriptors.cpp

    # Listings downloading classes
    HEADERS += datadirect.h
    SOURCES += datadirect.cpp

    # Simple NuppelVideo Recorder
    HEADERS += NuppelVideoRecorder.h       fifowriter.h
    SOURCES += NuppelVideoRecorder.cpp     fifowriter.cpp

    # Support for Video4Linux devices
    using_v4l {
        HEADERS += channel.h                   pchdtvsignalmonitor.h
        HEADERS += hdtvrecorder.h              analogscan.h
        SOURCES += channel.cpp                 pchdtvsignalmonitor.cpp
        SOURCES += hdtvrecorder.cpp            analogscan.cpp

        DEFINES += USING_V4L
    }

    # Support for cable boxes that provide Firewire out on Linux
    using_firewire:HEADERS += firewirerecorder.h   firewirechannel.h
    using_firewire:SOURCES += firewirerecorder.cpp firewirechannel.cpp
    using_firewire:DEFINES += USING_FIREWIRE

    # Support for set top boxes (Nokia DBox2 etc.)
    using_dbox2:SOURCES += dbox2recorder.cpp dbox2channel.cpp dbox2epg.cpp
    using_dbox2:HEADERS += dbox2recorder.h dbox2channel.h dbox2epg.h

    # Support for PVR-150/250/350/500, etc. on Linux
    using_ivtv:HEADERS += mpegrecorder.h
    using_ivtv:SOURCES += mpegrecorder.cpp
    using_ivtv:DEFINES += USING_IVTV
    using_ivtv_header:DEFINES += USING_IVTV_HEADER

    # Support for Linux DVB drivers
    using_dvb {
        # C files
        HEADERS += dvbdev/dvbdev.h   dvbdev/transform.h   dvbdev/ringbuffy.h
        SOURCES += dvbdev/dvbdev.c   dvbdev/transform.c   dvbdev/ringbuffy.c

        # Basic DVB types
        HEADERS += sitypes.h              dvbtypes.h
        SOURCES += sitypes.cpp            dvbtypes.cpp

        # Section/Table/Descriptor parsers
        HEADERS += siparser.h             dvbsiparser.h
        SOURCES += siparser.cpp           dvbsiparser.cpp

        # Channel stuff
        HEADERS += dvbchannel.h           dvbsignalmonitor.h
        HEADERS += dvbdiseqc.h            dvbcam.h
        SOURCES += dvbchannel.cpp         dvbsignalmonitor.cpp
        SOURCES += dvbdiseqc.cpp          dvbcam.cpp

        # DVB Recorder
        HEADERS += dvbrecorder.h
        SOURCES += dvbrecorder.cpp

        # Misc
        HEADERS += dvbconfparser.h        dvbdev/dvbci.h
        SOURCES += dvbconfparser.cpp      dvbdev/dvbci.cpp

        DEFINES += USING_DVB
        using_dvb_eit {
            HEADERS += eithelper.h   eitscanner.h   eitfixup.h   eit.h
            SOURCES += eithelper.cpp eitscanner.cpp eitfixup.cpp eit.cpp
            DEFINES += USING_DVB_EIT
        }
    }

    # C stuff
    HEADERS += frequencies.h
    SOURCES += frequencies.c

    DEFINES += USING_BACKEND
}

# Files used by frontend and backend.
using_frontend:HEADERS *= channeleditor.h      channelsettings.h
using_frontend:HEADERS *= previewgenerator.h   dvbtransporteditor.h
using_frontend:SOURCES *= channeleditor.cpp    channelsettings.cpp
using_frontend:SOURCES *= previewgenerator.cpp dvbtransporteditor.cpp
using_backend:HEADERS  *= channeleditor.h      channelsettings.h
using_backend:HEADERS  *= previewgenerator.h   dvbtransporteditor.h
using_backend:SOURCES  *= channeleditor.cpp    channelsettings.cpp
using_backend:SOURCES  *= previewgenerator.cpp dvbtransporteditor.cpp
