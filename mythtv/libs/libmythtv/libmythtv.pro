include ( ../../settings.pro )

TEMPLATE = lib
TARGET = mythtv-$$LIBVERSION
CONFIG += thread dll
target.path = $${PREFIX}/lib
INSTALLS = target

VERSION = 0.15.0

INCLUDEPATH += ../libmyth ../ dvbdev/ ../libavcodec
DEPENDPATH += ../libmyth ../libavcodec ../libavformat

LIBS += -L../libmyth -L../libavcodec -L../libavformat
LIBS += -lmyth-$${LIBVERSION} -lmythavcodec-$${LIBVERSION} -lmythavformat-$${LIBVERSION} $$EXTRA_LIBS

TARGETDEPS += ../libmyth/libmyth-$${LIBVERSION}.so
TARGETDEPS += ../libavcodec/libmythavcodec-$${LIBVERSION}.so
TARGETDEPS += ../libavformat/libmythavformat-$${LIBVERSION}.so

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
HEADERS += guidegrid.h infodialog.h infostructs.h jitterometer.h lzoconf.h 
HEADERS += minilzo.h mmx.h NuppelVideoPlayer.h NuppelVideoRecorder.h osd.h 
HEADERS += osdtypes.h programinfo.h profilegroup.h recordingprofile.h 
HEADERS += remoteencoder.h remoteutil.h RingBuffer.h scheduledrecording.h 
HEADERS += RTjpegN.h ttfont.h tv_play.h tv_rec.h videosource.h yuv2rgb.h
HEADERS += progfind.h decoderbase.h nuppeldecoder.h avformatdecoder.h
HEADERS += recorderbase.h channelbase.h vsync.h proglist.h hdtvrecorder.h 
HEADERS += fifowriter.h filtermanager.h videooutbase.h videoout_null.h xbox.h
HEADERS += dbcheck.h udpnotify.h channeleditor.h channelsettings.h
HEADERS += osdlistbtntype.h blend.h datadirect.h

SOURCES += commercial_skip.cpp frequencies.c guidegrid.cpp infodialog.cpp 
SOURCES += infostructs.cpp jitterometer.cpp minilzo.cpp NuppelVideoPlayer.cpp 
SOURCES += osd.cpp osdtypes.cpp programinfo.cpp recordingprofile.cpp 
SOURCES += remoteencoder.cpp remoteutil.cpp RingBuffer.cpp RTjpegN.cpp 
SOURCES += scheduledrecording.cpp ttfont.cpp tv_play.cpp videosource.cpp 
SOURCES += yuv2rgb.cpp progfind.cpp nuppeldecoder.cpp avformatdecoder.cpp 
SOURCES += recorderbase.cpp filtermanager.cpp proglist.cpp videooutbase.cpp 
SOURCES += videoout_null.cpp xbox.cpp dbcheck.cpp profilegroup.cpp
SOURCES += udpnotify.cpp channeleditor.cpp channelsettings.cpp
SOURCES += osdsurface.cpp osdlistbtntype.cpp blend.c datadirect.cpp

!win32 {
    HEADERS += channel.h
    SOURCES += channel.cpp NuppelVideoRecorder.cpp tv_rec.cpp channelbase.cpp
    SOURCES += vsync.c hdtvrecorder.cpp fifowriter.cpp
}

using_x11 {
    using_xv {
        SOURCES += videoout_xv.cpp
        HEADERS += videoout_xv.h
        DEFINES += USING_XV
    }

    using_xvmc {
        SOURCES += videoout_xvmc.cpp
        HEADERS += videoout_xvmc.h
    }
}

using_ivtv {
    SOURCES += mpegrecorder.cpp ivtvdecoder.cpp videoout_ivtv.cpp
    HEADERS += mpegrecorder.h ivtvdecoder.h videoout_ivtv.h
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

using_directx {
    SOURCES += videoout_dx.cpp
    HEADERS += videoout_dx.h
}

