include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.14.0

INCLUDEPATH += ../libmyth ../ dvbdev/ ../libavcodec
DEPENDPATH += ../libmyth ../libavcodec ../libavformat

TARGETDEPS += ../libmyth/libmyth-$${LIBVERSION}.so
TARGETDEPS += ../libavcodec/libmythavcodec-$${LIBVERSION}.so
TARGETDEPS += ../libavformat/libmythavformat-$${LIBVERSION}.so

QMAKE_CFLAGS_RELEASE = $$OPTFLAGS -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
QMAKE_CFLAGS_DEBUG = -g -DHAVE_AV_CONFIG_H -I.. -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE

QMAKE_CXXFLAGS_RELEASE += `freetype-config --cflags`
QMAKE_CXXFLAGS_DEBUG += `freetype-config --cflags`

# old libvbitext

HEADERS += vbitext/cc.h vbitext/dllist.h vbitext/hamm.h vbitext/lang.h 
HEADERS += vbitext/vbi.h vbitext/vt.h
SOURCES += vbitext/cc.cpp vbitext/vbi.c vbitext/hamm.c vbitext/lang.c


# libmythtv proper

HEADERS += channel.h commercial_skip.h filter.h format.h frame.h frequencies.h 
HEADERS += guidegrid.h infodialog.h infostructs.h jitterometer.h lzoconf.h 
HEADERS += minilzo.h mmx.h NuppelVideoPlayer.h NuppelVideoRecorder.h osd.h 
HEADERS += osdtypes.h programinfo.h profilegroup.h recordingprofile.h 
HEADERS += remoteencoder.h remoteutil.h RingBuffer.h scheduledrecording.h 
HEADERS += RTjpegN.h ttfont.h tv_play.h tv_rec.h videosource.h yuv2rgb.h
HEADERS += progfind.h decoderbase.h nuppeldecoder.h avformatdecoder.h
HEADERS += recorderbase.h mpegrecorder.h channelbase.h
HEADERS += vsync.h proglist.h hdtvrecorder.h fifowriter.h filtermanager.h
HEADERS += videooutbase.h videoout_null.h xbox.h
HEADERS += ivtvdecoder.h videoout_ivtv.h dbcheck.h udpnotify.h
HEADERS += channeleditor.h channelsettings.h

SOURCES += channel.cpp commercial_skip.cpp frequencies.c guidegrid.cpp
SOURCES += infodialog.cpp infostructs.cpp jitterometer.cpp minilzo.cpp 
SOURCES += NuppelVideoPlayer.cpp NuppelVideoRecorder.cpp osd.cpp
SOURCES += osdtypes.cpp programinfo.cpp recordingprofile.cpp remoteencoder.cpp
SOURCES += remoteutil.cpp RingBuffer.cpp RTjpegN.cpp scheduledrecording.cpp
SOURCES += ttfont.cpp tv_play.cpp tv_rec.cpp videosource.cpp yuv2rgb.cpp
SOURCES += progfind.cpp nuppeldecoder.cpp avformatdecoder.cpp recorderbase.cpp
SOURCES += mpegrecorder.cpp channelbase.cpp filtermanager.cpp
SOURCES += vsync.c proglist.cpp hdtvrecorder.cpp videooutbase.cpp 
SOURCES += fifowriter.cpp videoout_null.cpp xbox.cpp
SOURCES += ivtvdecoder.cpp videoout_ivtv.cpp dbcheck.cpp profilegroup.cpp
SOURCES += udpnotify.cpp channeleditor.cpp channelsettings.cpp

using_xv {
    SOURCES += videoout_xv.cpp
    HEADERS += videoout_xv.h
    DEFINES += USING_XV
}

using_xvmc {
    SOURCES += videoout_xvmc.cpp
    HEADERS += videoout_xvmc.h
}

using_viahwslice {
    SOURCES += videoout_viaslice.cpp
    HEADERS += videoout_viaslice.h
}

using_dvb {
    SOURCES += dvbrecorder.cpp dvbchannel.cpp dvbdiseqc.cpp dvbsections.cpp 
    SOURCES += dvbcam.cpp
    HEADERS += dvbtypes.h dvbrecorder.h dvbchannel.h dvbdiseqc.h
    HEADERS += dvbsections.h dvbcam.h

    SOURCES += dvbdev/dvbdev.c dvbdev/transform.c dvbdev/remux.c 
    SOURCES += dvbdev/ringbuffy.c dvbdev/ctools.c dvbdev/dvbci.cpp
    HEADERS += dvbdev/dvbdev.h dvbdev/transform.h dvbdev/remux.h 
    HEADERS += dvbdev/ringbuffy.h dvbdev/ctools.h dvbdev/dvbci.h
}

using_directfb {
    SOURCES += videoout_directfb.cpp
    HEADERS += videoout_directfb.h
    DEFINES += USING_DIRECTFB
}

