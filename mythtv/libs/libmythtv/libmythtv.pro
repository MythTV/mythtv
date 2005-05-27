include ( ../../config.mak )
include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.18.0

INCLUDEPATH += ../..
INCLUDEPATH += ../libmyth ../ ./dvbdev/ ./mpeg/ ../libavcodec ../libmythmpeg2
DEPENDPATH += ../libmyth ../libavcodec ../libavformat ../libmythmpeg2

LIBS += -L../libmyth -L../libavcodec -L../libavformat -L../libmythmpeg2
LIBS += -lmyth-$${LIBVERSION} -lmythavcodec-$${LIBVERSION} \
        -lmythavformat-$${LIBVERSION} -lmythmpeg2-$${LIBVERSION} \
        $$EXTRA_LIBS

isEmpty(QMAKE_EXTENSION_SHLIB) {
  QMAKE_EXTENSION_SHLIB=so
}
isEmpty(QMAKE_EXTENSION_LIB) {
  QMAKE_EXTENSION_LIB=a
}
TARGETDEPS += ../libmyth/libmyth-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libavcodec/libmythavcodec-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libavformat/libmythavformat-$${LIBVERSION}.$${QMAKE_EXTENSION_SHLIB}
TARGETDEPS += ../libmythmpeg2/libmythmpeg2-$${LIBVERSION}.$${QMAKE_EXTENSION_LIB}

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

QMAKE_CXXFLAGS_RELEASE += `freetype-config --cflags`
QMAKE_CXXFLAGS_DEBUG += `freetype-config --cflags`

# old libvbitext

!win32 {
    HEADERS += vbitext/cc.h vbitext/dllist.h vbitext/hamm.h vbitext/lang.h 
    HEADERS += vbitext/vbi.h vbitext/vt.h
    SOURCES += vbitext/cc.cpp vbitext/vbi.c vbitext/hamm.c vbitext/lang.c
}

# libmythtv proper

HEADERS += commercial_skip.h filter.h format.h frame.h frequencies.h 
HEADERS += guidegrid.h infostructs.h jitterometer.h lzoconf.h #infodialog.h
HEADERS += minilzo.h NuppelVideoPlayer.h NuppelVideoRecorder.h osd.h 
HEADERS += osdtypes.h programinfo.h profilegroup.h recordingprofile.h 
HEADERS += remoteencoder.h remoteutil.h RingBuffer.h scheduledrecording.h 
HEADERS += RTjpegN.h ttfont.h tv_play.h tv_rec.h videosource.h yuv2rgb.h
HEADERS += progfind.h decoderbase.h nuppeldecoder.h avformatdecoder.h
HEADERS += recorderbase.h channelbase.h vsync.h proglist.h hdtvrecorder.h 
HEADERS += fifowriter.h filtermanager.h videooutbase.h videoout_null.h xbox.h
HEADERS += dbcheck.h udpnotify.h channeleditor.h channelsettings.h
HEADERS += osdlistbtntype.h blend.h datadirect.h sr_dialog.h
HEADERS += sr_items.h sr_root.h recordingtypes.h jobqueue.h dtvrecorder.h
HEADERS += videobuffers.h

SOURCES += commercial_skip.cpp frequencies.c guidegrid.cpp #infodialog.cpp 
SOURCES += infostructs.cpp jitterometer.cpp minilzo.cpp NuppelVideoPlayer.cpp 
SOURCES += osd.cpp osdtypes.cpp programinfo.cpp recordingprofile.cpp 
SOURCES += remoteencoder.cpp remoteutil.cpp RingBuffer.cpp RTjpegN.cpp 
SOURCES += scheduledrecording.cpp ttfont.cpp tv_play.cpp videosource.cpp 
SOURCES += yuv2rgb.cpp progfind.cpp nuppeldecoder.cpp avformatdecoder.cpp 
SOURCES += recorderbase.cpp filtermanager.cpp vsync.cpp proglist.cpp videooutbase.cpp 
SOURCES += videoout_null.cpp xbox.cpp dbcheck.cpp profilegroup.cpp
SOURCES += udpnotify.cpp channeleditor.cpp channelsettings.cpp
SOURCES += osdsurface.cpp osdlistbtntype.cpp blend.c datadirect.cpp
SOURCES += sr_dialog.cpp sr_root.cpp sr_items.cpp decoderbase.cpp
SOURCES += recordingtypes.cpp jobqueue.cpp dtvrecorder.cpp
SOURCES += videobuffers.cpp


unix {
    # The backend uses these, but the frontend doesn't.
    # Comment out for a faster frontend-only build!
    HEADERS += channel.h
    HEADERS += mpeg/tsstats.h mpeg/tspacket.h mpeg/pespacket.h
    HEADERS += mpeg/mpegtables.h mpeg/atsctables.h
    HEADERS += mpeg/mpegstreamdata.h mpeg/atscdescriptors.h mpeg/atscstreamdata.h
    HEADERS += signalmonitorvalue.h signalmonitor.h dtvsignalmonitor.h
    HEADERS += pchdtvsignalmonitor.h

    SOURCES += channel.cpp NuppelVideoRecorder.cpp tv_rec.cpp channelbase.cpp
    SOURCES += hdtvrecorder.cpp fifowriter.cpp
    SOURCES += mpeg/tspacket.cpp mpeg/pespacket.cpp
    SOURCES += mpeg/mpegstreamdata.cpp mpeg/atscstreamdata.cpp
    SOURCES += mpeg/mpegtables.cpp mpeg/atsctables.cpp
    SOURCES += mpeg/atscdescriptors.cpp mpeg/atscdescriptorsmap.cpp
    SOURCES += signalmonitorvalue.cpp signalmonitor.cpp dtvsignalmonitor.cpp
    SOURCES += pchdtvsignalmonitor.cpp
}

macx {
    SOURCES += videoout_quartz.cpp
    HEADERS += videoout_quartz.h

    # Mac OS X Frameworks
    FWKS = ApplicationServices Carbon QuickTime

    # The following trick shortens the command line, but depends on
    # the shell expanding Csh-style braces. Luckily, Bash and Zsh do.
    FC = $$join(FWKS,",","{","}")

    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")

    QMAKE_LFLAGS_SHLIB += -seg1addr 0xC9000000
}

using_x11 {
    using_xvmc:DEFINES += USING_XVMC
    using_xvmcw:DEFINES += USING_XVMCW
    using_xvmc_vld:DEFINES += USING_XVMC_VLD

    using_xv {
        SOURCES += videoout_xv.cpp XvMCSurfaceTypes.cpp osdxvmc.cpp
        HEADERS += videoout_xv.h XvMCSurfaceTypes.h osdxvmc.h
        DEFINES += USING_XV
    }
}

using_ivtv {
    DEFINES += USING_IVTV
    SOURCES += mpegrecorder.cpp ivtvdecoder.cpp videoout_ivtv.cpp
    HEADERS += mpegrecorder.h ivtvdecoder.h videoout_ivtv.h
}
using_ivtv_header:DEFINES += USING_IVTV_HEADER

using_dvb {
    DEFINES += USING_DVB
    using_dvb_eit:DEFINES += USING_DVB_EIT
    SOURCES += dvbrecorder.cpp dvbchannel.cpp dvbdiseqc.cpp dvbcam.cpp
    SOURCES += dvbtransporteditor.cpp dvbsiparser.cpp siparser.cpp siscan.cpp
    SOURCES += scanwizard.cpp dvbsignalmonitor.cpp sitypes.cpp dvbtypes.cpp
    SOURCES += dvbdev/dvbdev.c dvbdev/transform.c dvbdev/ringbuffy.c 
    SOURCES += dvbdev/dvbci.cpp

    HEADERS += dvbtypes.h dvbrecorder.h dvbchannel.h dvbdiseqc.h dvbcam.h
    HEADERS += dvbtransporteditor.h dvbsiparser.h siparser.h siscan.h
    HEADERS += scanwizard.h dvbsignalmonitor.h sitypes.h
    HEADERS += dvbdev/dvbdev.h dvbdev/transform.h dvbdev/ringbuffy.h 
    HEADERS += dvbdev/dvbci.h
}

using_firewire {
    DEFINES += USING_FIREWIRE
    SOURCES += firewirerecorder.cpp firewirechannel.cpp
    HEADERS += firewirerecorder.h firewirechannel.h
}

using_directfb {
    SOURCES += videoout_directfb.cpp
    HEADERS += videoout_directfb.h
    DEFINES += USING_DIRECTFB
}

using_directx {
    SOURCES += videoout_dx.cpp
    HEADERS += videoout_dx.h
}

using_opengl_vsync {
    CONFIG += opengl
    DEFINES += USING_OPENGL_VSYNC
}

using_oss:DEFINES += USING_OSS

contains( TARGET_MMX, yes ) {
    HEADERS += ../../libs/libavcodec/i386/mmx.h ../../libs/libavcodec/dsputil.h
}
